#ifndef CANFRAME_H
#define CANFRAME_H

#include <QString>
#include <optional>

inline constexpr quint32 maxStandardCanId = 0x7FF;
inline constexpr qsizetype maxClassicCanPayload = 8;

inline constexpr quint32 canCommandId = 0x650;

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

#endif // CANFRAME_H
