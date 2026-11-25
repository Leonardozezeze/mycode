#ifndef CANFRAME_H
#define CANFRAME_H

#include <QRegularExpression>
#include <QString>
#include <optional>

inline constexpr quint32 maxStandardCanId = 0x7FF;
inline constexpr qsizetype maxClassicCanPayload = 8;

inline constexpr quint32 defaultCanCommandId = 0x401;

inline std::optional<quint32> parseStandardCanId(const QString &text)
{
    const auto idText = text.trimmed();
    const bool hex = idText.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive);
    bool ok = false;
    const auto id = (hex ? idText.mid(2) : idText).toUInt(&ok, hex ? 16 : 10);
    if (!ok || id > maxStandardCanId)
        return std::nullopt;
    return id;
}

inline std::optional<quint32> parseStandardHexCanId(const QString &text)
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
