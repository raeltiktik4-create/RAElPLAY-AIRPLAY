#ifndef RAELPLAY_JSON_DECODER_H
#define RAELPLAY_JSON_DECODER_H

#include <stddef.h>
#include <stdio.h>

#define RAELPLAY_JSON_DECODER_MAX_BYTES 4096
#define RAELPLAY_JSON_DECODER_MAX_DEPTH (RAELPLAY_JSON_DECODER_MAX_BYTES / 2)

typedef struct {
    const unsigned char *current;
    const unsigned char *end;
} RaelplayJsonParser;

static void raelplay_json_skip_whitespace(RaelplayJsonParser *parser) {
    while (parser->current < parser->end &&
           (*parser->current == ' ' || *parser->current == '\t' ||
            *parser->current == '\r' || *parser->current == '\n')) {
        parser->current++;
    }
}

static int raelplay_json_read_utf8(RaelplayJsonParser *parser, unsigned int *codepoint) {
    const unsigned char *p = parser->current;
    unsigned char first;

    if (p >= parser->end) return 0;
    first = *p++;
    if (first < 0x80) {
        *codepoint = first;
        parser->current = p;
        return 1;
    }

    if (first >= 0xC2 && first <= 0xDF) {
        if (parser->end - p < 1 || (p[0] & 0xC0) != 0x80) return 0;
        *codepoint = ((unsigned int)(first & 0x1F) << 6) | (p[0] & 0x3F);
        parser->current = p + 1;
        return 1;
    }

    if (first >= 0xE0 && first <= 0xEF) {
        if (parser->end - p < 2 || (p[0] & 0xC0) != 0x80 || (p[1] & 0xC0) != 0x80)
            return 0;
        if ((first == 0xE0 && p[0] < 0xA0) || (first == 0xED && p[0] >= 0xA0))
            return 0;
        *codepoint = ((unsigned int)(first & 0x0F) << 12) |
                     ((unsigned int)(p[0] & 0x3F) << 6) | (p[1] & 0x3F);
        parser->current = p + 2;
        return 1;
    }

    if (first >= 0xF0 && first <= 0xF4) {
        if (parser->end - p < 3 || (p[0] & 0xC0) != 0x80 ||
            (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80)
            return 0;
        if ((first == 0xF0 && p[0] < 0x90) || (first == 0xF4 && p[0] > 0x8F))
            return 0;
        *codepoint = ((unsigned int)(first & 0x07) << 18) |
                     ((unsigned int)(p[0] & 0x3F) << 12) |
                     ((unsigned int)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        parser->current = p + 3;
        return 1;
    }

    return 0;
}

static int raelplay_json_read_string(RaelplayJsonParser *parser, int *is_decoder) {
    static const char decoder_key[] = "decoder";
    size_t key_position = 0;
    int matches_decoder = 1;

    if (parser->current >= parser->end || *parser->current++ != '"') return 0;
    while (parser->current < parser->end) {
        unsigned int codepoint;
        unsigned char c = *parser->current;

        if (c == '"') {
            parser->current++;
            *is_decoder = matches_decoder && key_position == sizeof decoder_key - 1;
            return 1;
        }
        if (c < 0x20) return 0;

        if (c == '\\') {
            parser->current++;
            if (parser->current >= parser->end) return 0;
            c = *parser->current++;
            switch (c) {
            case '"': codepoint = '"'; break;
            case '\\': codepoint = '\\'; break;
            case '/': codepoint = '/'; break;
            case 'b': codepoint = '\b'; break;
            case 'f': codepoint = '\f'; break;
            case 'n': codepoint = '\n'; break;
            case 'r': codepoint = '\r'; break;
            case 't': codepoint = '\t'; break;
            case 'u': {
                unsigned int value = 0;
                int i;
                for (i = 0; i < 4; i++) {
                    unsigned char h;
                    if (parser->current >= parser->end) return 0;
                    h = *parser->current++;
                    if (h >= '0' && h <= '9') value = (value << 4) | (h - '0');
                    else if (h >= 'a' && h <= 'f') value = (value << 4) | (h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') value = (value << 4) | (h - 'A' + 10);
                    else return 0;
                }
                codepoint = value;
                break;
            }
            default: return 0;
            }
        } else if (c < 0x80) {
            codepoint = c;
            parser->current++;
        } else if (!raelplay_json_read_utf8(parser, &codepoint)) {
            return 0;
        }

        if (matches_decoder) {
            if (key_position >= sizeof decoder_key - 1 ||
                codepoint != (unsigned char)decoder_key[key_position]) {
                matches_decoder = 0;
            } else {
                key_position++;
            }
        }
    }

    return 0;
}

static int raelplay_json_read_number(RaelplayJsonParser *parser, int *is_decoder_value, int *value) {
    const unsigned char *start = parser->current;
    const unsigned char *p = start;

    if (p < parser->end && *p == '-') p++;
    if (p >= parser->end) return 0;
    if (*p == '0') {
        p++;
    } else if (*p >= '1' && *p <= '9') {
        do { p++; } while (p < parser->end && *p >= '0' && *p <= '9');
    } else {
        return 0;
    }

    if (p < parser->end && *p == '.') {
        p++;
        if (p >= parser->end || *p < '0' || *p > '9') return 0;
        do { p++; } while (p < parser->end && *p >= '0' && *p <= '9');
    }
    if (p < parser->end && (*p == 'e' || *p == 'E')) {
        p++;
        if (p < parser->end && (*p == '+' || *p == '-')) p++;
        if (p >= parser->end || *p < '0' || *p > '9') return 0;
        do { p++; } while (p < parser->end && *p >= '0' && *p <= '9');
    }

    parser->current = p;
    *is_decoder_value = (size_t)(p - start) == 1 && *start >= '0' && *start <= '2';
    *value = *is_decoder_value ? (int)(*start - '0') : 2;
    return 1;
}

static int raelplay_json_parse_value(RaelplayJsonParser *parser, size_t depth,
                                     int capture_decoder, int *decoder_value_valid,
                                     int *decoder_value);

static int raelplay_json_parse_object(RaelplayJsonParser *parser, size_t depth, int root,
                                      int *decoder_seen, int *decoder_duplicate,
                                      int *decoder_value_valid, int *decoder_value) {
    if (depth > RAELPLAY_JSON_DECODER_MAX_DEPTH) return 0;
    parser->current++;
    raelplay_json_skip_whitespace(parser);
    if (parser->current < parser->end && *parser->current == '}') {
        parser->current++;
        return 1;
    }

    for (;;) {
        int is_decoder_key = 0;
        int capture_valid = 0, capture_value = 2;

        if (!raelplay_json_read_string(parser, &is_decoder_key)) return 0;
        raelplay_json_skip_whitespace(parser);
        if (parser->current >= parser->end || *parser->current++ != ':') return 0;
        raelplay_json_skip_whitespace(parser);

        if (root && is_decoder_key) {
            if (*decoder_seen) *decoder_duplicate = 1;
            *decoder_seen = 1;
            if (!raelplay_json_parse_value(parser, depth, 1, &capture_valid, &capture_value))
                return 0;
            *decoder_value_valid = capture_valid;
            *decoder_value = capture_value;
        } else {
            if (!raelplay_json_parse_value(parser, depth, 0, &capture_valid, &capture_value))
                return 0;
        }

        raelplay_json_skip_whitespace(parser);
        if (parser->current >= parser->end) return 0;
        if (*parser->current == '}') {
            parser->current++;
            return 1;
        }
        if (*parser->current++ != ',') return 0;
        raelplay_json_skip_whitespace(parser);
    }
}

static int raelplay_json_parse_value(RaelplayJsonParser *parser, size_t depth,
                                     int capture_decoder, int *decoder_value_valid,
                                     int *decoder_value) {
    static const char *literals[] = { "true", "false", "null" };
    int ignored_key = 0;

    if (parser->current >= parser->end) return 0;
    if (capture_decoder) *decoder_value_valid = 0;

    switch (*parser->current) {
    case '"':
        return raelplay_json_read_string(parser, &ignored_key);
    case '{':
        return raelplay_json_parse_object(parser, depth + 1, 0, NULL, NULL, NULL, NULL);
    case '[':
        if (depth + 1 > RAELPLAY_JSON_DECODER_MAX_DEPTH) return 0;
        parser->current++;
        raelplay_json_skip_whitespace(parser);
        if (parser->current < parser->end && *parser->current == ']') {
            parser->current++;
            return 1;
        }
        for (;;) {
            int ignored_valid = 0, ignored_value = 2;
            if (!raelplay_json_parse_value(parser, depth + 1, 0, &ignored_valid, &ignored_value))
                return 0;
            raelplay_json_skip_whitespace(parser);
            if (parser->current >= parser->end) return 0;
            if (*parser->current == ']') {
                parser->current++;
                return 1;
            }
            if (*parser->current++ != ',') return 0;
            raelplay_json_skip_whitespace(parser);
        }
    case 't':
    case 'f':
    case 'n': {
        const char *literal = *parser->current == 't' ? literals[0] :
                              (*parser->current == 'f' ? literals[1] : literals[2]);
        size_t length = *parser->current == 't' ? 4 :  (*parser->current == 'f' ? 5 : 4);
        if ((size_t)(parser->end - parser->current) < length ||
            memcmp(parser->current, literal, length) != 0)
            return 0;
        parser->current += length;
        return 1;
    }
    default:
        if (*parser->current == '-' || (*parser->current >= '0' && *parser->current <= '9')) {
            int valid = 0, value = 2;
            if (!raelplay_json_read_number(parser, &valid, &value)) return 0;
            if (capture_decoder) {
                *decoder_value_valid = valid;
                *decoder_value = value;
            }
            return 1;
        }
        return 0;
    }
}

static int raelplay_json_decoder_from_bytes(const unsigned char *bytes, size_t length) {
    RaelplayJsonParser parser;
    int decoder_seen = 0, decoder_duplicate = 0;
    int decoder_value_valid = 0, decoder_value = 2;

    if (!bytes || length > RAELPLAY_JSON_DECODER_MAX_BYTES) return 2;
    if (length >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
        bytes += 3;
        length -= 3;
    }
    parser.current = bytes;
    parser.end = bytes + length;
    raelplay_json_skip_whitespace(&parser);
    if (parser.current >= parser.end || *parser.current != '{') return 2;
    if (!raelplay_json_parse_object(&parser, 0, 1, &decoder_seen, &decoder_duplicate,
                                    &decoder_value_valid, &decoder_value))
        return 2;
    raelplay_json_skip_whitespace(&parser);
    if (parser.current != parser.end || !decoder_seen || decoder_duplicate ||
        !decoder_value_valid)
        return 2;
    return decoder_value;
}

static int raelplay_json_decoder_from_file(const char *path) {
    unsigned char bytes[RAELPLAY_JSON_DECODER_MAX_BYTES];
    FILE *file;
    long length;
    size_t read_length;
    int result = 2;

    file = fopen(path, "rb");
    if (!file) return 2;
    if (fseek(file, 0, SEEK_END) == 0) {
        length = ftell(file);
        if (length >= 0 && length <= RAELPLAY_JSON_DECODER_MAX_BYTES &&
            fseek(file, 0, SEEK_SET) == 0) {
            read_length = fread(bytes, 1, (size_t)length, file);
            if (read_length == (size_t)length && !ferror(file))
                result = raelplay_json_decoder_from_bytes(bytes, read_length);
        }
    }
    fclose(file);
    return result;
}

#endif
