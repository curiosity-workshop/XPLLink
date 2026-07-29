#include <xpllink/profile/JsonDeviceProfileStore.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <variant>

namespace xpllink::profile
{
    namespace
    {
        struct JsonValue;

        using JsonArray = std::vector<JsonValue>;
        using JsonObject = std::map<std::string, JsonValue>;

        struct JsonValue
        {
            using Value = std::variant<
                std::nullptr_t,
                bool,
                double,
                std::string,
                JsonArray,
                JsonObject>;

            Value value;
        };

        class JsonParser
        {
        public:
            explicit JsonParser(std::string_view text)
                : text_(text)
            {
            }

            std::optional<JsonValue> parse()
            {
                skipWhitespace();
                auto value = parseValue();
                skipWhitespace();

                if (!value || position_ != text_.size())
                {
                    return std::nullopt;
                }

                return value;
            }

        private:
            std::optional<JsonValue> parseValue()
            {
                skipWhitespace();

                if (position_ >= text_.size())
                {
                    return std::nullopt;
                }

                const char current = text_[position_];

                if (current == '"')
                {
                    auto value = parseString();
                    if (!value)
                    {
                        return std::nullopt;
                    }

                    return JsonValue{ *value };
                }

                if (current == '{')
                {
                    return parseObject();
                }

                if (current == '[')
                {
                    return parseArray();
                }

                if (match("true"))
                {
                    return JsonValue{ true };
                }

                if (match("false"))
                {
                    return JsonValue{ false };
                }

                if (match("null"))
                {
                    return JsonValue{ nullptr };
                }

                return parseNumber();
            }

            std::optional<JsonValue> parseObject()
            {
                if (!consume('{'))
                {
                    return std::nullopt;
                }

                JsonObject object;
                skipWhitespace();

                if (consume('}'))
                {
                    return JsonValue{ object };
                }

                while (position_ < text_.size())
                {
                    auto key = parseString();
                    if (!key)
                    {
                        return std::nullopt;
                    }

                    skipWhitespace();
                    if (!consume(':'))
                    {
                        return std::nullopt;
                    }

                    auto value = parseValue();
                    if (!value)
                    {
                        return std::nullopt;
                    }

                    object.emplace(std::move(*key), std::move(*value));
                    skipWhitespace();

                    if (consume('}'))
                    {
                        return JsonValue{ object };
                    }

                    if (!consume(','))
                    {
                        return std::nullopt;
                    }

                    skipWhitespace();
                }

                return std::nullopt;
            }

            std::optional<JsonValue> parseArray()
            {
                if (!consume('['))
                {
                    return std::nullopt;
                }

                JsonArray array;
                skipWhitespace();

                if (consume(']'))
                {
                    return JsonValue{ array };
                }

                while (position_ < text_.size())
                {
                    auto value = parseValue();
                    if (!value)
                    {
                        return std::nullopt;
                    }

                    array.push_back(std::move(*value));
                    skipWhitespace();

                    if (consume(']'))
                    {
                        return JsonValue{ array };
                    }

                    if (!consume(','))
                    {
                        return std::nullopt;
                    }
                }

                return std::nullopt;
            }

            std::optional<std::string> parseString()
            {
                if (!consume('"'))
                {
                    return std::nullopt;
                }

                std::string value;

                while (position_ < text_.size())
                {
                    const char current = text_[position_++];

                    if (current == '"')
                    {
                        return value;
                    }

                    if (current != '\\')
                    {
                        value.push_back(current);
                        continue;
                    }

                    if (position_ >= text_.size())
                    {
                        return std::nullopt;
                    }

                    const char escaped = text_[position_++];

                    switch (escaped)
                    {
                    case '"':
                    case '\\':
                    case '/':
                        value.push_back(escaped);
                        break;

                    case 'b':
                        value.push_back('\b');
                        break;

                    case 'f':
                        value.push_back('\f');
                        break;

                    case 'n':
                        value.push_back('\n');
                        break;

                    case 'r':
                        value.push_back('\r');
                        break;

                    case 't':
                        value.push_back('\t');
                        break;

                    default:
                        return std::nullopt;
                    }
                }

                return std::nullopt;
            }

            std::optional<JsonValue> parseNumber()
            {
                const std::size_t start = position_;

                if (position_ < text_.size() &&
                    text_[position_] == '-')
                {
                    ++position_;
                }

                while (position_ < text_.size() &&
                    std::isdigit(
                        static_cast<unsigned char>(text_[position_])))
                {
                    ++position_;
                }

                if (position_ < text_.size() &&
                    text_[position_] == '.')
                {
                    ++position_;

                    while (position_ < text_.size() &&
                        std::isdigit(
                            static_cast<unsigned char>(text_[position_])))
                    {
                        ++position_;
                    }
                }

                if (position_ == start ||
                    (position_ == start + 1 && text_[start] == '-'))
                {
                    return std::nullopt;
                }

                const auto token =
                    std::string{ text_.substr(start, position_ - start) };

                try
                {
                    return JsonValue{ std::stod(token) };
                }
                catch (...)
                {
                    return std::nullopt;
                }
            }

            void skipWhitespace()
            {
                while (position_ < text_.size() &&
                    std::isspace(
                        static_cast<unsigned char>(text_[position_])))
                {
                    ++position_;
                }
            }

            bool consume(char expected)
            {
                skipWhitespace();

                if (position_ >= text_.size() ||
                    text_[position_] != expected)
                {
                    return false;
                }

                ++position_;
                return true;
            }

            bool match(std::string_view token)
            {
                if (text_.substr(position_, token.size()) != token)
                {
                    return false;
                }

                position_ += token.size();
                return true;
            }

            std::string_view text_;
            std::size_t position_ = 0;
        };

        std::string escapeJsonString(std::string_view value)
        {
            std::ostringstream escaped;

            for (const char ch : value)
            {
                switch (ch)
                {
                case '"':
                    escaped << "\\\"";
                    break;

                case '\\':
                    escaped << "\\\\";
                    break;

                case '\b':
                    escaped << "\\b";
                    break;

                case '\f':
                    escaped << "\\f";
                    break;

                case '\n':
                    escaped << "\\n";
                    break;

                case '\r':
                    escaped << "\\r";
                    break;

                case '\t':
                    escaped << "\\t";
                    break;

                default:
                    escaped << ch;
                    break;
                }
            }

            return escaped.str();
        }

        std::string sanitizeFileName(std::string value)
        {
            for (char& ch : value)
            {
                const bool keep =
                    std::isalnum(static_cast<unsigned char>(ch)) ||
                    ch == '-' ||
                    ch == '_';

                if (!keep)
                {
                    ch = '_';
                }
            }

            while (value.find("__") != std::string::npos)
            {
                const auto position = value.find("__");
                value.replace(position, 2, "_");
            }

            if (value.empty())
            {
                return "unknown_device";
            }

            return value;
        }

        const JsonObject* asObject(const JsonValue& value)
        {
            return std::get_if<JsonObject>(&value.value);
        }

        const JsonArray* asArray(const JsonValue& value)
        {
            return std::get_if<JsonArray>(&value.value);
        }

        const std::string* asString(const JsonValue& value)
        {
            return std::get_if<std::string>(&value.value);
        }

        const bool* asBool(const JsonValue& value)
        {
            return std::get_if<bool>(&value.value);
        }

        std::optional<int> asInt(const JsonValue& value)
        {
            const auto* number = std::get_if<double>(&value.value);
            if (number == nullptr)
            {
                return std::nullopt;
            }

            return static_cast<int>(*number);
        }

        std::optional<long> asLong(const JsonValue& value)
        {
            const auto* number = std::get_if<double>(&value.value);
            if (number == nullptr)
            {
                return std::nullopt;
            }

            return static_cast<long>(*number);
        }

        const JsonValue* find(
            const JsonObject& object,
            std::string_view key)
        {
            const auto item =
                object.find(std::string{ key });

            if (item == object.end())
            {
                return nullptr;
            }

            return &item->second;
        }

        void writeStringProperty(
            std::ostream& stream,
            int indent,
            std::string_view name,
            std::string_view value,
            bool comma = true)
        {
            stream
                << std::string(static_cast<std::size_t>(indent), ' ')
                << '"'
                << name
                << "\": \""
                << escapeJsonString(value)
                << '"'
                << (comma ? "," : "")
                << '\n';
        }

        void writeIntProperty(
            std::ostream& stream,
            int indent,
            std::string_view name,
            long value,
            bool comma = true)
        {
            stream
                << std::string(static_cast<std::size_t>(indent), ' ')
                << '"'
                << name
                << "\": "
                << value
                << (comma ? "," : "")
                << '\n';
        }

        void writeBoolProperty(
            std::ostream& stream,
            int indent,
            std::string_view name,
            bool value,
            bool comma = true)
        {
            stream
                << std::string(static_cast<std::size_t>(indent), ' ')
                << '"'
                << name
                << "\": "
                << (value ? "true" : "false")
                << (comma ? "," : "")
                << '\n';
        }

        void writeOptionalIntProperty(
            std::ostream& stream,
            int indent,
            std::string_view name,
            const std::optional<int>& value,
            bool comma = true)
        {
            stream
                << std::string(static_cast<std::size_t>(indent), ' ')
                << '"'
                << name
                << "\": ";

            if (value)
            {
                stream << *value;
            }
            else
            {
                stream << "null";
            }

            stream
                << (comma ? "," : "")
                << '\n';
        }

        void writeUpdate(
            std::ostream& stream,
            const DeviceProfileUpdateSubscription& update,
            bool comma)
        {
            stream << "        {\n";
            writeIntProperty(stream, 10, "handle", update.handle);
            writeOptionalIntProperty(stream, 10, "requestedType", update.requestedType);
            writeIntProperty(stream, 10, "rate", update.rate);
            writeStringProperty(stream, 10, "precision", update.precision);
            writeOptionalIntProperty(stream, 10, "element", update.element, false);
            stream
                << "        }"
                << (comma ? "," : "")
                << '\n';
        }

        void writeDataRef(
            std::ostream& stream,
            const DeviceProfileDataRef& dataRef,
            bool comma)
        {
            stream << "    {\n";
            writeIntProperty(stream, 6, "handle", dataRef.handle);
            writeStringProperty(stream, 6, "name", dataRef.name);
            writeIntProperty(stream, 6, "xplaneType", dataRef.xplaneType);
            writeBoolProperty(stream, 6, "active", dataRef.active);
            writeBoolProperty(stream, 6, "scalingActive", dataRef.scalingActive);
            writeIntProperty(stream, 6, "scaleFromLow", dataRef.scaleFromLow);
            writeIntProperty(stream, 6, "scaleFromHigh", dataRef.scaleFromHigh);
            writeIntProperty(stream, 6, "scaleToLow", dataRef.scaleToLow);
            writeIntProperty(stream, 6, "scaleToHigh", dataRef.scaleToHigh);
            stream << "      \"updates\": [\n";

            for (std::size_t index = 0; index < dataRef.updates.size(); ++index)
            {
                writeUpdate(
                    stream,
                    dataRef.updates[index],
                    index + 1 < dataRef.updates.size());
            }

            stream << "      ]\n";
            stream
                << "    }"
                << (comma ? "," : "")
                << '\n';
        }

        void writeCommand(
            std::ostream& stream,
            const DeviceProfileCommand& command,
            bool comma)
        {
            stream << "    {\n";
            writeIntProperty(stream, 6, "handle", command.handle);
            writeStringProperty(stream, 6, "name", command.name);
            writeBoolProperty(stream, 6, "active", command.active, false);
            stream
                << "    }"
                << (comma ? "," : "")
                << '\n';
        }

        std::optional<DeviceProfileUpdateSubscription> parseUpdate(
            const JsonObject& object)
        {
            DeviceProfileUpdateSubscription update;

            if (const auto* value = find(object, "handle"))
            {
                update.handle = asInt(*value).value_or(update.handle);
            }

            if (const auto* value = find(object, "requestedType"))
            {
                update.requestedType = asInt(*value);
            }

            if (const auto* value = find(object, "rate"))
            {
                update.rate = asInt(*value).value_or(update.rate);
            }

            if (const auto* value = find(object, "precision"))
            {
                if (const auto* precision = asString(*value))
                {
                    update.precision = *precision;
                }
            }

            if (const auto* value = find(object, "element"))
            {
                update.element = asInt(*value);
            }

            return update;
        }

        std::optional<DeviceProfileDataRef> parseDataRef(
            const JsonObject& object)
        {
            DeviceProfileDataRef dataRef;

            if (const auto* value = find(object, "handle"))
            {
                dataRef.handle = asInt(*value).value_or(dataRef.handle);
            }

            if (const auto* value = find(object, "name"))
            {
                if (const auto* name = asString(*value))
                {
                    dataRef.name = *name;
                }
            }

            if (const auto* value = find(object, "xplaneType"))
            {
                dataRef.xplaneType =
                    asInt(*value).value_or(dataRef.xplaneType);
            }

            if (const auto* value = find(object, "active"))
            {
                if (const auto* active = asBool(*value))
                {
                    dataRef.active = *active;
                }
            }

            if (const auto* value = find(object, "scalingActive"))
            {
                if (const auto* scalingActive = asBool(*value))
                {
                    dataRef.scalingActive = *scalingActive;
                }
            }

            if (const auto* value = find(object, "scaleFromLow"))
            {
                dataRef.scaleFromLow =
                    asLong(*value).value_or(dataRef.scaleFromLow);
            }

            if (const auto* value = find(object, "scaleFromHigh"))
            {
                dataRef.scaleFromHigh =
                    asLong(*value).value_or(dataRef.scaleFromHigh);
            }

            if (const auto* value = find(object, "scaleToLow"))
            {
                dataRef.scaleToLow =
                    asLong(*value).value_or(dataRef.scaleToLow);
            }

            if (const auto* value = find(object, "scaleToHigh"))
            {
                dataRef.scaleToHigh =
                    asLong(*value).value_or(dataRef.scaleToHigh);
            }

            if (const auto* value = find(object, "updates"))
            {
                if (const auto* updates = asArray(*value))
                {
                    for (const auto& item : *updates)
                    {
                        if (const auto* updateObject = asObject(item))
                        {
                            if (auto update = parseUpdate(*updateObject))
                            {
                                dataRef.updates.push_back(std::move(*update));
                            }
                        }
                    }
                }
            }

            return dataRef;
        }

        std::optional<DeviceProfileCommand> parseCommand(
            const JsonObject& object)
        {
            DeviceProfileCommand command;

            if (const auto* value = find(object, "handle"))
            {
                command.handle = asInt(*value).value_or(command.handle);
            }

            if (const auto* value = find(object, "name"))
            {
                if (const auto* name = asString(*value))
                {
                    command.name = *name;
                }
            }

            if (const auto* value = find(object, "active"))
            {
                if (const auto* active = asBool(*value))
                {
                    command.active = *active;
                }
            }

            return command;
        }
    }

    JsonDeviceProfileStore::JsonDeviceProfileStore(
        std::filesystem::path profileDirectory)
        : profileDirectory_(std::move(profileDirectory))
    {
    }

    const std::filesystem::path&
        JsonDeviceProfileStore::profileDirectory() const
    {
        return profileDirectory_;
    }

    std::filesystem::path JsonDeviceProfileStore::profilePathFor(
        const DeviceProfile& profile) const
    {
        const std::string stem =
            sanitizeFileName(
                profile.deviceName + "__" + profile.deviceVersion);

        return profileDirectory_ / (stem + ".json");
    }

    bool JsonDeviceProfileStore::save(
        const DeviceProfile& profile) const
    {
        std::error_code error;
        std::filesystem::create_directories(
            profileDirectory_,
            error);

        if (error)
        {
            return false;
        }

        std::ofstream stream{
            profilePathFor(profile),
            std::ios::binary |
            std::ios::trunc
        };

        if (!stream)
        {
            return false;
        }

        stream << "{\n";
        writeIntProperty(
            stream,
            2,
            "cacheSchemaVersion",
            profile.cacheSchemaVersion);
        writeStringProperty(stream, 2, "protocol", profile.protocol);
        writeStringProperty(stream, 2, "deviceName", profile.deviceName);
        writeStringProperty(stream, 2, "deviceVersion", profile.deviceVersion);
        stream << "  \"dataRefs\": [\n";

        for (std::size_t index = 0; index < profile.dataRefs.size(); ++index)
        {
            writeDataRef(
                stream,
                profile.dataRefs[index],
                index + 1 < profile.dataRefs.size());
        }

        stream << "  ],\n";
        stream << "  \"commands\": [\n";

        for (std::size_t index = 0; index < profile.commands.size(); ++index)
        {
            writeCommand(
                stream,
                profile.commands[index],
                index + 1 < profile.commands.size());
        }

        stream << "  ]\n";
        stream << "}\n";

        return stream.good();
    }

    std::optional<DeviceProfile> JsonDeviceProfileStore::load(
        const std::filesystem::path& path) const
    {
        std::ifstream stream{
            path,
            std::ios::binary
        };

        if (!stream)
        {
            return std::nullopt;
        }

        std::ostringstream buffer;
        buffer << stream.rdbuf();

        const std::string text =
            buffer.str();

        JsonParser parser{ text };
        auto root = parser.parse();

        if (!root)
        {
            return std::nullopt;
        }

        const auto* object = asObject(*root);
        if (object == nullptr)
        {
            return std::nullopt;
        }

        DeviceProfile profile;

        if (const auto* value = find(*object, "cacheSchemaVersion"))
        {
            profile.cacheSchemaVersion =
                asInt(*value).value_or(profile.cacheSchemaVersion);
        }

        if (const auto* value = find(*object, "protocol"))
        {
            if (const auto* protocol = asString(*value))
            {
                profile.protocol = *protocol;
            }
        }

        if (const auto* value = find(*object, "deviceName"))
        {
            if (const auto* deviceName = asString(*value))
            {
                profile.deviceName = *deviceName;
            }
        }

        if (const auto* value = find(*object, "deviceVersion"))
        {
            if (const auto* deviceVersion = asString(*value))
            {
                profile.deviceVersion = *deviceVersion;
            }
        }

        if (const auto* value = find(*object, "dataRefs"))
        {
            if (const auto* dataRefs = asArray(*value))
            {
                for (const auto& item : *dataRefs)
                {
                    if (const auto* dataRefObject = asObject(item))
                    {
                        if (auto dataRef = parseDataRef(*dataRefObject))
                        {
                            profile.dataRefs.push_back(std::move(*dataRef));
                        }
                    }
                }
            }
        }

        if (const auto* value = find(*object, "commands"))
        {
            if (const auto* commands = asArray(*value))
            {
                for (const auto& item : *commands)
                {
                    if (const auto* commandObject = asObject(item))
                    {
                        if (auto command = parseCommand(*commandObject))
                        {
                            profile.commands.push_back(std::move(*command));
                        }
                    }
                }
            }
        }

        return profile;
    }
}
