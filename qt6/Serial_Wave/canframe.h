#ifndef CANFRAME_H
#define CANFRAME_H

#include <QRegularExpression>
#include <QString>
#include <optional>

inline constexpr quint32 maxStandardCanId = 0x7FF;
inline constexpr qsizetype maxClassicCanPayload = 8;

inline constexpr quint32 defaultCanCommandId = 0x401;
inline constexpr quint32 defaultCanReplyId = 0x402;
inline constexpr quint32 defaultCanQualityId = 0x100;
inline constexpr quint32 defaultCanSpeedId = 0x101;
inline constexpr quint32 defaultCanActuatorId = 0x102;

inline std::optional<quint32> parseStandardCanId(const QString &text)
{
    const auto idText = text.trimmed();
    const bool hex = idText.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive);
    static const QRegularExpression idPattern(QStringLiteral("\\A(?:0[xX][0-9A-Fa-f]+|[0-9]+)\\z"));
    if (!idPattern.match(idText).hasMatch())
        return std::nullopt;
    bool ok = false;
    const auto id = (hex ? idText.mid(2) : idText).toUInt(&ok, hex ? 16 : 10);
    if (!ok || id > maxStandardCanId)
        return std::nullopt;
    return id;
}

inline std::optional<quint32> parseLegacyHexCanId(const QString &text)
{
    static const QRegularExpression hexId(QStringLiteral("\\A(?:0[xX])?[0-9A-Fa-f]+\\z"));
    const auto trimmed = text.trimmed();
    bool ok = false;
    const auto id = trimmed.toUInt(&ok, 16);
    if (!ok || id > maxStandardCanId || !hexId.match(trimmed).hasMatch())
        return std::nullopt;
    return id;
}

inline QString formatStandardCanId(quint32 id)
{
    return QStringLiteral("0x") + QString::number(id, 16).rightJustified(3, QLatin1Char('0')).toUpper();
}

#endif // CANFRAME_H
