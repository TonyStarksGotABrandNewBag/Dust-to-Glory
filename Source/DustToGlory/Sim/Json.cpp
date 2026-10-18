#include "Sim/Json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace dtg::sim
{
	namespace detail_json
	{
		const JsonValue& SharedNull()
		{
			static const JsonValue Null;
			return Null;
		}

		const std::string& SharedEmptyString()
		{
			static const std::string Empty;
			return Empty;
		}

		void AppendUtf8(std::string& Out, std::uint32_t CodePoint)
		{
			if (CodePoint < 0x80u)
			{
				Out.push_back(static_cast<char>(CodePoint));
			}
			else if (CodePoint < 0x800u)
			{
				Out.push_back(static_cast<char>(0xC0u | (CodePoint >> 6u)));
				Out.push_back(static_cast<char>(0x80u | (CodePoint & 0x3Fu)));
			}
			else if (CodePoint < 0x10000u)
			{
				Out.push_back(static_cast<char>(0xE0u | (CodePoint >> 12u)));
				Out.push_back(static_cast<char>(0x80u | ((CodePoint >> 6u) & 0x3Fu)));
				Out.push_back(static_cast<char>(0x80u | (CodePoint & 0x3Fu)));
			}
			else
			{
				Out.push_back(static_cast<char>(0xF0u | (CodePoint >> 18u)));
				Out.push_back(static_cast<char>(0x80u | ((CodePoint >> 12u) & 0x3Fu)));
				Out.push_back(static_cast<char>(0x80u | ((CodePoint >> 6u) & 0x3Fu)));
				Out.push_back(static_cast<char>(0x80u | (CodePoint & 0x3Fu)));
			}
		}

		class Parser
		{
		public:
			explicit Parser(const std::string& InText) : Text(InText) {}

			bool ParseDocument(JsonValue& Out)
			{
				SkipWhitespace();
				if (!ParseValue(Out, 0))
				{
					return false;
				}
				SkipWhitespace();
				if (Pos != Text.size())
				{
					return Fail("Unexpected trailing characters");
				}
				return true;
			}

			std::string Error;
			std::size_t ErrorPos = 0;

		private:
			static constexpr int MaxDepth = 128;

			bool Fail(const char* Message)
			{
				if (Error.empty())
				{
					Error = Message;
					ErrorPos = Pos;
				}
				return false;
			}

			void SkipWhitespace()
			{
				while (Pos < Text.size())
				{
					const char Character = Text[Pos];
					if (Character == ' ' || Character == '\t' || Character == '\n' || Character == '\r')
					{
						++Pos;
					}
					else if (Character == '/' && Pos + 1 < Text.size() && Text[Pos + 1] == '/')
					{
						while (Pos < Text.size() && Text[Pos] != '\n')
						{
							++Pos;
						}
					}
					else
					{
						break;
					}
				}
			}

			bool Match(const char* Literal)
			{
				std::size_t Index = 0;
				while (Literal[Index] != '\0')
				{
					if (Pos + Index >= Text.size() || Text[Pos + Index] != Literal[Index])
					{
						return false;
					}
					++Index;
				}
				Pos += Index;
				return true;
			}

			bool ParseValue(JsonValue& Out, int Depth)
			{
				if (Depth > MaxDepth)
				{
					return Fail("Nesting too deep");
				}
				if (Pos >= Text.size())
				{
					return Fail("Unexpected end of input");
				}
				const char Character = Text[Pos];
				if (Character == '{')
				{
					return ParseObject(Out, Depth);
				}
				if (Character == '[')
				{
					return ParseArray(Out, Depth);
				}
				if (Character == '"')
				{
					std::string Value;
					if (!ParseString(Value))
					{
						return false;
					}
					Out = JsonValue::MakeString(std::move(Value));
					return true;
				}
				if (Match("true"))
				{
					Out = JsonValue::MakeBool(true);
					return true;
				}
				if (Match("false"))
				{
					Out = JsonValue::MakeBool(false);
					return true;
				}
				if (Match("null"))
				{
					Out = JsonValue();
					return true;
				}
				return ParseNumber(Out);
			}

			bool ParseNumber(JsonValue& Out)
			{
				const std::size_t Start = Pos;
				if (Pos < Text.size() && Text[Pos] == '-')
				{
					++Pos;
				}
				bool bDigits = false;
				while (Pos < Text.size() && Text[Pos] >= '0' && Text[Pos] <= '9')
				{
					++Pos;
					bDigits = true;
				}
				if (Pos < Text.size() && Text[Pos] == '.')
				{
					++Pos;
					while (Pos < Text.size() && Text[Pos] >= '0' && Text[Pos] <= '9')
					{
						++Pos;
						bDigits = true;
					}
				}
				if (bDigits && Pos < Text.size() && (Text[Pos] == 'e' || Text[Pos] == 'E'))
				{
					++Pos;
					if (Pos < Text.size() && (Text[Pos] == '+' || Text[Pos] == '-'))
					{
						++Pos;
					}
					while (Pos < Text.size() && Text[Pos] >= '0' && Text[Pos] <= '9')
					{
						++Pos;
					}
				}
				if (!bDigits)
				{
					Pos = Start;
					return Fail("Invalid value");
				}
				const std::string Slice = Text.substr(Start, Pos - Start);
				char* End = nullptr;
				const double Value = std::strtod(Slice.c_str(), &End);
				if (End == Slice.c_str())
				{
					return Fail("Invalid number");
				}
				Out = JsonValue::MakeNumber(Value);
				return true;
			}

			bool ParseHex4(std::uint32_t& Out)
			{
				if (Pos + 4 > Text.size())
				{
					return Fail("Truncated unicode escape");
				}
				std::uint32_t Value = 0;
				for (int Index = 0; Index < 4; ++Index)
				{
					const char Character = Text[Pos++];
					Value <<= 4u;
					if (Character >= '0' && Character <= '9')
					{
						Value |= static_cast<std::uint32_t>(Character - '0');
					}
					else if (Character >= 'a' && Character <= 'f')
					{
						Value |= static_cast<std::uint32_t>(Character - 'a' + 10);
					}
					else if (Character >= 'A' && Character <= 'F')
					{
						Value |= static_cast<std::uint32_t>(Character - 'A' + 10);
					}
					else
					{
						return Fail("Invalid unicode escape");
					}
				}
				Out = Value;
				return true;
			}

			bool ParseString(std::string& Out)
			{
				++Pos; // opening quote
				while (Pos < Text.size())
				{
					const char Character = Text[Pos++];
					if (Character == '"')
					{
						return true;
					}
					if (Character == '\\')
					{
						if (Pos >= Text.size())
						{
							break;
						}
						const char Escape = Text[Pos++];
						switch (Escape)
						{
						case '"': Out.push_back('"'); break;
						case '\\': Out.push_back('\\'); break;
						case '/': Out.push_back('/'); break;
						case 'b': Out.push_back('\b'); break;
						case 'f': Out.push_back('\f'); break;
						case 'n': Out.push_back('\n'); break;
						case 'r': Out.push_back('\r'); break;
						case 't': Out.push_back('\t'); break;
						case 'u':
						{
							std::uint32_t CodePoint = 0;
							if (!ParseHex4(CodePoint))
							{
								return false;
							}
							if (CodePoint >= 0xD800u && CodePoint <= 0xDBFFu)
							{
								std::uint32_t Low = 0;
								if (!Match("\\u") || !ParseHex4(Low) || Low < 0xDC00u || Low > 0xDFFFu)
								{
									return Fail("Invalid surrogate pair");
								}
								CodePoint = 0x10000u + ((CodePoint - 0xD800u) << 10u) + (Low - 0xDC00u);
							}
							AppendUtf8(Out, CodePoint);
							break;
						}
						default:
							return Fail("Invalid escape sequence");
						}
					}
					else if (static_cast<unsigned char>(Character) < 0x20u)
					{
						return Fail("Control character in string");
					}
					else
					{
						Out.push_back(Character);
					}
				}
				return Fail("Unterminated string");
			}

			bool ParseArray(JsonValue& Out, int Depth)
			{
				++Pos;
				Out = JsonValue::MakeArray();
				SkipWhitespace();
				if (Pos < Text.size() && Text[Pos] == ']')
				{
					++Pos;
					return true;
				}
				while (true)
				{
					SkipWhitespace();
					JsonValue Element;
					if (!ParseValue(Element, Depth + 1))
					{
						return false;
					}
					Out.Push(std::move(Element));
					SkipWhitespace();
					if (Pos < Text.size() && Text[Pos] == ',')
					{
						++Pos;
						continue;
					}
					if (Pos < Text.size() && Text[Pos] == ']')
					{
						++Pos;
						return true;
					}
					return Fail("Expected ',' or ']' in array");
				}
			}

			bool ParseObject(JsonValue& Out, int Depth)
			{
				++Pos;
				Out = JsonValue::MakeObject();
				SkipWhitespace();
				if (Pos < Text.size() && Text[Pos] == '}')
				{
					++Pos;
					return true;
				}
				while (true)
				{
					SkipWhitespace();
					if (Pos >= Text.size() || Text[Pos] != '"')
					{
						return Fail("Expected string key in object");
					}
					std::string Key;
					if (!ParseString(Key))
					{
						return false;
					}
					SkipWhitespace();
					if (Pos >= Text.size() || Text[Pos] != ':')
					{
						return Fail("Expected ':' after key");
					}
					++Pos;
					SkipWhitespace();
					JsonValue Member;
					if (!ParseValue(Member, Depth + 1))
					{
						return false;
					}
					Out.Set(Key, std::move(Member));
					SkipWhitespace();
					if (Pos < Text.size() && Text[Pos] == ',')
					{
						++Pos;
						continue;
					}
					if (Pos < Text.size() && Text[Pos] == '}')
					{
						++Pos;
						return true;
					}
					return Fail("Expected ',' or '}' in object");
				}
			}

			const std::string& Text;
			std::size_t Pos = 0;
		};

		void AppendIndent(std::string& Out, int Depth)
		{
			Out.push_back('\n');
			Out.append(static_cast<std::size_t>(Depth) * 2u, ' ');
		}
	}

	JsonValue JsonValue::MakeBool(bool Value)
	{
		JsonValue Result;
		Result.Kind = Type::Bool;
		Result.BoolValue = Value;
		return Result;
	}

	JsonValue JsonValue::MakeNumber(double Value)
	{
		JsonValue Result;
		Result.Kind = Type::Number;
		Result.NumberValue = Value;
		return Result;
	}

	JsonValue JsonValue::MakeString(std::string Value)
	{
		JsonValue Result;
		Result.Kind = Type::String;
		Result.StringValue = std::move(Value);
		return Result;
	}

	JsonValue JsonValue::MakeArray()
	{
		JsonValue Result;
		Result.Kind = Type::Array;
		return Result;
	}

	JsonValue JsonValue::MakeObject()
	{
		JsonValue Result;
		Result.Kind = Type::Object;
		return Result;
	}

	int JsonValue::AsInt(int Fallback) const
	{
		if (Kind != Type::Number || !std::isfinite(NumberValue))
		{
			return Fallback;
		}
		const double Rounded = std::round(NumberValue);
		if (Rounded > 2147483647.0 || Rounded < -2147483648.0)
		{
			return Fallback;
		}
		return static_cast<int>(Rounded);
	}

	std::int64_t JsonValue::AsInt64(std::int64_t Fallback) const
	{
		if (Kind != Type::Number || !std::isfinite(NumberValue))
		{
			return Fallback;
		}
		const double Rounded = std::round(NumberValue);
		if (Rounded > 9.0e18 || Rounded < -9.0e18)
		{
			return Fallback;
		}
		return static_cast<std::int64_t>(Rounded);
	}

	const std::string& JsonValue::AsString() const
	{
		return Kind == Type::String ? StringValue : detail_json::SharedEmptyString();
	}

	std::size_t JsonValue::Size() const
	{
		if (Kind == Type::Array)
		{
			return ArrayValue.size();
		}
		if (Kind == Type::Object)
		{
			return ObjectValue.size();
		}
		return 0;
	}

	const JsonValue& JsonValue::At(std::size_t Index) const
	{
		if (Kind != Type::Array || Index >= ArrayValue.size())
		{
			return detail_json::SharedNull();
		}
		return ArrayValue[Index];
	}

	const JsonValue* JsonValue::Find(const std::string& Key) const
	{
		if (Kind != Type::Object)
		{
			return nullptr;
		}
		for (const auto& Member : ObjectValue)
		{
			if (Member.first == Key)
			{
				return &Member.second;
			}
		}
		return nullptr;
	}

	const JsonValue& JsonValue::Get(const std::string& Key) const
	{
		const JsonValue* Found = Find(Key);
		return Found ? *Found : detail_json::SharedNull();
	}

	std::string JsonValue::GetString(const std::string& Key, const std::string& Fallback) const
	{
		const JsonValue* Found = Find(Key);
		return (Found && Found->IsString()) ? Found->StringValue : Fallback;
	}

	double JsonValue::GetNumber(const std::string& Key, double Fallback) const
	{
		const JsonValue* Found = Find(Key);
		return Found ? Found->AsNumber(Fallback) : Fallback;
	}

	int JsonValue::GetInt(const std::string& Key, int Fallback) const
	{
		const JsonValue* Found = Find(Key);
		return Found ? Found->AsInt(Fallback) : Fallback;
	}

	std::int64_t JsonValue::GetInt64(const std::string& Key, std::int64_t Fallback) const
	{
		const JsonValue* Found = Find(Key);
		return Found ? Found->AsInt64(Fallback) : Fallback;
	}

	bool JsonValue::GetBool(const std::string& Key, bool Fallback) const
	{
		const JsonValue* Found = Find(Key);
		return Found ? Found->AsBool(Fallback) : Fallback;
	}

	JsonValue& JsonValue::Set(const std::string& Key, JsonValue Value)
	{
		if (Kind != Type::Object)
		{
			*this = MakeObject();
		}
		for (auto& Member : ObjectValue)
		{
			if (Member.first == Key)
			{
				Member.second = std::move(Value);
				return Member.second;
			}
		}
		ObjectValue.emplace_back(Key, std::move(Value));
		return ObjectValue.back().second;
	}

	JsonValue& JsonValue::Push(JsonValue Value)
	{
		if (Kind != Type::Array)
		{
			*this = MakeArray();
		}
		ArrayValue.push_back(std::move(Value));
		return ArrayValue.back();
	}

	std::string JsonQuote(const std::string& Text)
	{
		std::string Out;
		Out.reserve(Text.size() + 2);
		Out.push_back('"');
		for (const char Character : Text)
		{
			switch (Character)
			{
			case '"': Out += "\\\""; break;
			case '\\': Out += "\\\\"; break;
			case '\n': Out += "\\n"; break;
			case '\r': Out += "\\r"; break;
			case '\t': Out += "\\t"; break;
			case '\b': Out += "\\b"; break;
			case '\f': Out += "\\f"; break;
			default:
				if (static_cast<unsigned char>(Character) < 0x20u)
				{
					char Buffer[8];
					std::snprintf(Buffer, sizeof(Buffer), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(Character)));
					Out += Buffer;
				}
				else
				{
					Out.push_back(Character);
				}
			}
		}
		Out.push_back('"');
		return Out;
	}

	void JsonValue::DumpTo(std::string& Out, bool bPretty, int Depth) const
	{
		switch (Kind)
		{
		case Type::Null:
			Out += "null";
			break;
		case Type::Bool:
			Out += BoolValue ? "true" : "false";
			break;
		case Type::Number:
		{
			if (!std::isfinite(NumberValue))
			{
				Out += "0";
				break;
			}
			const double Rounded = std::round(NumberValue);
			char Buffer[40];
			if (Rounded == NumberValue && std::fabs(NumberValue) < 9.0e15)
			{
				std::snprintf(Buffer, sizeof(Buffer), "%lld", static_cast<long long>(Rounded));
			}
			else
			{
				std::snprintf(Buffer, sizeof(Buffer), "%.17g", NumberValue);
			}
			Out += Buffer;
			break;
		}
		case Type::String:
			Out += JsonQuote(StringValue);
			break;
		case Type::Array:
		{
			Out.push_back('[');
			for (std::size_t Index = 0; Index < ArrayValue.size(); ++Index)
			{
				if (Index != 0)
				{
					Out.push_back(',');
				}
				if (bPretty)
				{
					detail_json::AppendIndent(Out, Depth + 1);
				}
				ArrayValue[Index].DumpTo(Out, bPretty, Depth + 1);
			}
			if (bPretty && !ArrayValue.empty())
			{
				detail_json::AppendIndent(Out, Depth);
			}
			Out.push_back(']');
			break;
		}
		case Type::Object:
		{
			Out.push_back('{');
			for (std::size_t Index = 0; Index < ObjectValue.size(); ++Index)
			{
				if (Index != 0)
				{
					Out.push_back(',');
				}
				if (bPretty)
				{
					detail_json::AppendIndent(Out, Depth + 1);
				}
				Out += JsonQuote(ObjectValue[Index].first);
				Out += bPretty ? ": " : ":";
				ObjectValue[Index].second.DumpTo(Out, bPretty, Depth + 1);
			}
			if (bPretty && !ObjectValue.empty())
			{
				detail_json::AppendIndent(Out, Depth);
			}
			Out.push_back('}');
			break;
		}
		}
	}

	std::string JsonValue::Dump(bool bPretty) const
	{
		std::string Out;
		DumpTo(Out, bPretty, 0);
		return Out;
	}

	JsonParseResult ParseJson(const std::string& Text)
	{
		JsonParseResult Result;
		detail_json::Parser TheParser(Text);
		Result.bOk = TheParser.ParseDocument(Result.Value);
		if (!Result.bOk)
		{
			Result.Error = TheParser.Error;
			std::size_t Line = 1;
			std::size_t Column = 1;
			for (std::size_t Index = 0; Index < TheParser.ErrorPos && Index < Text.size(); ++Index)
			{
				if (Text[Index] == '\n')
				{
					++Line;
					Column = 1;
				}
				else
				{
					++Column;
				}
			}
			Result.Line = Line;
			Result.Column = Column;
			Result.Value = JsonValue();
		}
		return Result;
	}
}
