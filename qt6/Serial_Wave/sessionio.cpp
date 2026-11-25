#include "sessionio.h"
#include "samplearchive.h"
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>
#include <QtNumeric>
#include <algorithm>
#include <cmath>
#include <limits>

ViewData readSessionView(const QString &path, double lower, double upper,
                         const std::shared_ptr<std::atomic_bool> &cancelled)
{
    ViewData result;
    SampleArchive archive;
    if (!archive.open(path))
    {
        result.error = archive.errorString();
        return result;
    }
    archive.lastX(result.archiveLastTime);
    // 每桶保留首尾及每一路的极值，长记录也不生成无限大的绘图容器。
    constexpr int buckets = 400;
    // 首尾、各通道极值，以及每个绘图通道的首/末缺失点。
    // 保留 NaN，避免缺失反馈经过抽稀后被错误地连接成连续曲线。
    constexpr int missingBase = 2 + 2 * sampleChannelCount;
    std::array<SamplePoint, missingBase + 2 * waveformChannelCount> candidates{};
    std::array<bool, waveformChannelCount> hasMissing{};
    bool hasBucket = false;
    int previousBucket = -1;
    auto finishBucket = [&]
    {
        if (!hasBucket)
            return;
        std::sort(candidates.begin(), candidates.end(),
                  [](const auto &a, const auto &b) { return a.time < b.time; });
        for (const auto &p : candidates)
            if (result.points.isEmpty() || p.time != result.points.last().time)
                result.points.append(p);
    };
    const double span = qMax(upper - lower, 1e-9);
    double previousTime = -1;
    const bool ok = archive.forEachSampleInRange(
        lower, upper,
        [&](double x, const double *values)
        {
            if (cancelled->load())
                return false;
            SamplePoint p;
            p.time = x;
            std::copy_n(values, archive.channelCount(), p.values.begin());
            // 只标记原始数据中的实际间断，避免把抽稀后的点间距误当成丢数据。
            // 宽视图中小于一桶的间断不单独展开，控制绘图内存上限。
            if (previousTime >= 0 && x - previousTime > qMax(2.0, span / buckets))
            {
                finishBucket();
                hasBucket = false;
                p.breakBefore = true;
            }
            previousTime = x;
            const int bucket = qBound(0, int((x - lower) / span * buckets), buckets - 1);
            if (!hasBucket || bucket != previousBucket)
            {
                finishBucket();
                candidates.fill(p);
                for (int c = 0; c < waveformChannelCount; ++c)
                    hasMissing[c] = !qIsFinite(p.values[c]);
                hasBucket = true;
                previousBucket = bucket;
            }
            else
            {
                candidates[1] = p;
                for (int c = 0; c < waveformChannelCount; ++c)
                    if (!qIsFinite(p.values[c]))
                    {
                        if (!hasMissing[c])
                            candidates[missingBase + c * 2] = p;
                        candidates[missingBase + c * 2 + 1] = p;
                        hasMissing[c] = true;
                    }
                for (int c = 0; c < archive.channelCount(); ++c)
                {
                    if (qIsFinite(values[c]) && (!qIsFinite(candidates[2 + c * 2].values[c]) ||
                        values[c] < candidates[2 + c * 2].values[c]))
                        candidates[2 + c * 2] = p;
                    if (qIsFinite(values[c]) && (!qIsFinite(candidates[3 + c * 2].values[c]) ||
                        values[c] > candidates[3 + c * 2].values[c]))
                        candidates[3 + c * 2] = p;
                }
            }
            ++result.count;
            for (int c = 0; c < archive.channelCount(); ++c)
            {
                if (!qIsFinite(values[c]))
                    continue;
                auto &s = result.stats[c];
                ++s.count;
                if (s.count == 1)
                    s.min = s.max = values[c];
                s.min = qMin(s.min, values[c]);
                s.max = qMax(s.max, values[c]);
                const double delta = values[c] - s.mean;
                s.mean += delta / double(s.count);
                s.m2 += delta * (values[c] - s.mean);
            }
            return true;
        });
    finishBucket();
    if (!ok)
        result.error = archive.errorString();
    return result;
}

ExportResult exportSession(const QString &source, const QString &destination,
                           const std::shared_ptr<ExportState> &state)
{
    ExportResult result;
    if (state->selectedRange && (!qIsFinite(state->lower) || !qIsFinite(state->upper) ||
        state->lower < 0 || state->upper < state->lower))
    {
        result.error = QStringLiteral("导出时间区间无效");
        return result;
    }
    const QFileInfo src(source), dst(destination);
    if (src.absoluteFilePath() == dst.absoluteFilePath() ||
        (!dst.canonicalFilePath().isEmpty() && src.canonicalFilePath() == dst.canonicalFilePath()))
    {
        result.error = QStringLiteral("导出文件不能覆盖原始会话");
        return result;
    }
    SampleArchive archive;
    if (!archive.open(source))
    {
        result.error = archive.errorString();
        return result;
    }
    QSaveFile file(destination);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        result.error = file.errorString();
        return result;
    }
    QTextStream out(&file);
    const bool json = dst.suffix().compare(QStringLiteral("json"), Qt::CaseInsensitive) == 0;
    constexpr const char *keys[] = {"crush_rate", "impurity_rate", "motor_speed", "target_speed",
                                    "actuator_position_pct", "actuator_state"};
    if (json)
    {
        out << "{\n  \"version\": " << (archive.channelCount() == 4 ? 1 : 2)
            << ",\n  \"time_source\": \"host_monotonic_receive\",\n  "
               "\"started_at_utc\": \""
            << archive.startedAt().toString(Qt::ISODateWithMs) << "\",\n";
        if (state->selectedRange)
            out << "  \"range_start_s\": " << QString::number(state->lower, 'g', 17)
                << ",\n  \"range_end_s\": " << QString::number(state->upper, 'g', 17) << ",\n";
        out << "  \"samples\": [\n";
    }
    else
    {
        out << "time_s";
        for (int c = 0; c < archive.channelCount(); ++c)
            out << ',' << keys[c];
        out << ",received_at_utc\n";
    }
    qint64 count = 0;
    const bool readOk = archive.forEachSampleInRange(
        state->selectedRange ? state->lower : 0,
        state->selectedRange ? state->upper : std::numeric_limits<double>::max(),
        [&](double x, const double *v)
        {
            if (state->cancelled.load() || count >= state->total)
                return false;
            if (json)
            {
                if (count)
                    out << ",\n";
                out << "    {\"time_s\":" << QString::number(x, 'g', 17);
                for (int c = 0; c < archive.channelCount(); ++c)
                    out << ",\"" << keys[c] << "\":"
                        << (qIsFinite(v[c]) ? QString::number(v[c], 'g', 17)
                                            : QStringLiteral("null"));
                out << '}';
            }
            else
            {
                out << QString::number(x, 'g', 17);
                for (int c = 0; c < archive.channelCount(); ++c)
                    out << ',' << (qIsFinite(v[c]) ? QString::number(v[c], 'g', 17) : QString());
                out << ','
                    << archive.startedAt().addMSecs(qRound64(x * 1000)).toString(Qt::ISODateWithMs)
                    << '\n';
            }
            ++count;
            state->done.store(count);
            return out.status() == QTextStream::Ok;
        });
    if (json)
        out << "\n  ]\n}\n";
    out.flush();
    if (state->cancelled.load())
    {
        file.cancelWriting();
        result.cancelled = true;
        return result;
    }
    if (!readOk || count != state->total || out.status() != QTextStream::Ok)
    {
        result.error = !readOk ? archive.errorString() : QStringLiteral("导出不完整或文件写入失败");
        file.cancelWriting();
        return result;
    }
    if (!file.commit())
    {
        result.error = file.errorString();
        return result;
    }
    result.ok = true;
    return result;
}

MeasurementData readSessionMeasurement(const QString &path, double a, bool hasB, double b,
                                       const std::shared_ptr<std::atomic_bool> &cancelled)
{
    MeasurementData result;
    SampleArchive archive;
    if (!archive.open(path))
    {
        result.error = archive.errorString();
        return result;
    }
    QVector<double> values;
    if (!archive.nearestSample(a, result.a.time, values))
    {
        result.error = archive.errorString();
        return result;
    }
    std::copy(values.cbegin(), values.cend(), result.a.values.begin());
    result.hasA = true;
    if (!hasB || cancelled->load()) return result;
    if (!archive.nearestSample(b, result.b.time, values))
    {
        result.error = archive.errorString();
        return result;
    }
    std::copy(values.cbegin(), values.cend(), result.b.values.begin());
    result.hasB = true;
    const bool ok = archive.forEachSampleInRange(qMin(result.a.time, result.b.time),
        qMax(result.a.time, result.b.time), [&](double, const double *v)
        {
            if (cancelled->load()) return false;
            ++result.count;
            for (int c = 0; c < archive.channelCount(); ++c)
            {
                if (!qIsFinite(v[c])) continue;
                auto &s = result.stats[c];
                if (!s.count) s.min = s.max = v[c];
                ++s.count;
                s.min = qMin(s.min, v[c]); s.max = qMax(s.max, v[c]);
                const double delta = v[c] - s.mean;
                s.mean += delta / double(s.count);
                s.m2 += delta * (v[c] - s.mean);
            }
            return true;
        });
    if (!ok) result.error = archive.errorString();
    return result;
}
