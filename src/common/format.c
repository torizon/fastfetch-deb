#include "fastfetch.h"
#include "common/format.h"
#include "common/parsing.h"
#include "util/textModifier.h"
#include "util/stringUtils.h"

#include <inttypes.h>

void ffFormatAppendFormatArg(FFstrbuf* buffer, const FFformatarg* formatarg)
{
    if(formatarg->type == FF_FORMAT_ARG_TYPE_INT)
        ffStrbufAppendF(buffer, "%i", *(int*)formatarg->value);
    else if(formatarg->type == FF_FORMAT_ARG_TYPE_UINT)
        ffStrbufAppendF(buffer, "%" PRIu32, *(uint32_t*)formatarg->value);
    else if(formatarg->type == FF_FORMAT_ARG_TYPE_UINT64)
        ffStrbufAppendF(buffer, "%" PRIu64, *(uint64_t*)formatarg->value);
    else if(formatarg->type == FF_FORMAT_ARG_TYPE_UINT16)
        ffStrbufAppendF(buffer, "%" PRIu16, *(uint16_t*)formatarg->value);
    else if(formatarg->type == FF_FORMAT_ARG_TYPE_UINT8)
        ffStrbufAppendF(buffer, "%" PRIu8, *(uint8_t*)formatarg->value);
    else if(formatarg->type == FF_FORMAT_ARG_TYPE_STRING)
        ffStrbufAppendS(buffer, (const char*)formatarg->value);
    else if(formatarg->type == FF_FORMAT_ARG_TYPE_STRBUF)
        ffStrbufAppend(buffer, (FFstrbuf*)formatarg->value);
    else if(formatarg->type == FF_FORMAT_ARG_TYPE_FLOAT)
        ffStrbufAppendF(buffer, "%f", *(float*)formatarg->value);
    else if(formatarg->type == FF_FORMAT_ARG_TYPE_DOUBLE)
        ffStrbufAppendF(buffer, "%g", *(double*)formatarg->value);
    else if(formatarg->type == FF_FORMAT_ARG_TYPE_BOOL)
        ffStrbufAppendS(buffer, *(bool*)formatarg->value ? "true" : "false");
    else if(formatarg->type == FF_FORMAT_ARG_TYPE_LIST)
    {
        const FFlist* list = formatarg->value;
        for(uint32_t i = 0; i < list->length; i++)
        {
            ffStrbufAppend(buffer, ffListGet(list, i));
            if(i < list->length - 1)
                ffStrbufAppendS(buffer, ", ");
        }
    }
    else if(formatarg->type != FF_FORMAT_ARG_TYPE_NULL)
    {
        fprintf(stderr, "Error: format string \"%s\": argument is not implemented: %i\n", buffer->chars, formatarg->type);
    }
}

/**
 * @brief parses a string to a uint32_t
 *
 * If the string can't be parsed, or is < 1, uint32_t max is returned.
 *
 * @param placeholderValue the string to parse
 * @return uint32_t the parsed value
 */
static inline uint32_t getArgumentIndex(const FFstrbuf* placeholderValue)
{
    uint32_t result = UINT32_MAX;

    if(placeholderValue->chars[0] != '-')
        sscanf(placeholderValue->chars, "%" PRIu32, &result);

    return result == 0 ? UINT32_MAX : result;
}

static inline void appendInvalidPlaceholder(FFstrbuf* buffer, const char* start, const FFstrbuf* placeholderValue, uint32_t index, uint32_t formatStringLength)
{
    ffStrbufAppendS(buffer, start);
    ffStrbufAppend(buffer, placeholderValue);

    if(index < formatStringLength)
        ffStrbufAppendC(buffer, '}');
}

static inline void appendEmptyPlaceholder(FFstrbuf* buffer, const char* placeholder, uint32_t* argCounter, uint32_t numArgs, const FFformatarg* arguments)
{
    if(*argCounter >= numArgs)
        ffStrbufAppendS(buffer, placeholder);
    else
        ffFormatAppendFormatArg(buffer, arguments + *argCounter);

    (*argCounter)++;
}

static inline bool formatArgSet(const FFformatarg* arg)
{
    return arg->value != NULL && (
        (arg->type == FF_FORMAT_ARG_TYPE_DOUBLE && *(double*)arg->value > 0.0) || //Also is false for NaN
        (arg->type == FF_FORMAT_ARG_TYPE_INT && *(int*)arg->value > 0) ||
        (arg->type == FF_FORMAT_ARG_TYPE_STRBUF && ((FFstrbuf*)arg->value)->length > 0) ||
        (arg->type == FF_FORMAT_ARG_TYPE_STRING && ffStrSet(arg->value)) ||
        (arg->type == FF_FORMAT_ARG_TYPE_UINT8 && *(uint8_t*)arg->value > 0) ||
        (arg->type == FF_FORMAT_ARG_TYPE_UINT16 && *(uint16_t*)arg->value > 0) ||
        (arg->type == FF_FORMAT_ARG_TYPE_UINT && *(uint32_t*)arg->value > 0) ||
        (arg->type == FF_FORMAT_ARG_TYPE_BOOL && arg->value != NULL)
    );
}

void ffParseFormatString(FFstrbuf* buffer, const FFstrbuf* formatstr, uint32_t numArgs, const FFformatarg* arguments)
{
    uint32_t argCounter = 0;

    uint32_t numOpenIfs = 0;
    uint32_t numOpenNotIfs = 0;
    uint32_t numOpenColors = 0;

    for(uint32_t i = 0; i < formatstr->length; ++i)
    {
        // if we don't have a placeholder start just copy the chars over to output buffer
        if(formatstr->chars[i] != '{')
        {
            ffStrbufAppendC(buffer, formatstr->chars[i]);
            continue;
        }

        // if we have an { at the end handle it as {}
        if(i == formatstr->length - 1)
        {
            appendEmptyPlaceholder(buffer, "{", &argCounter, numArgs, arguments);
            continue;
        }

        // jump to next char, the start of the placeholder value
        ++i;

        // double {{ elvaluates to a single { and doesn't count as start
        if(formatstr->chars[i] == '{')
        {
            ffStrbufAppendC(buffer, '{');
            continue;
        }

        // placeholder is {}
        if(formatstr->chars[i] == '}')
        {
            appendEmptyPlaceholder(buffer, "{}", &argCounter, numArgs, arguments);
            continue;
        }

        FF_STRBUF_AUTO_DESTROY placeholderValue = ffStrbufCreate();

        while(i < formatstr->length && formatstr->chars[i] != '}')
            ffStrbufAppendC(&placeholderValue, formatstr->chars[i++]);

         // test if for stop, if so break the loop
        if(placeholderValue.length == 1 && placeholderValue.chars[0] == '-')
            break;

        // test for end of an if, if so do nothing
        if(placeholderValue.length == 1 && placeholderValue.chars[0] == '?')
        {
            if(numOpenIfs == 0)
                appendInvalidPlaceholder(buffer, "{", &placeholderValue, i, formatstr->length);
            else
                --numOpenIfs;

            continue;
        }

        // test for end of a not if, if so do nothing
        if(placeholderValue.length == 1 && placeholderValue.chars[0] == '/')
        {
            if(numOpenNotIfs == 0)
                appendInvalidPlaceholder(buffer, "{", &placeholderValue, i, formatstr->length);
            else
                --numOpenNotIfs;

            continue;
        }

        // test for end of a color, if so do nothing
        if(placeholderValue.length == 1 && placeholderValue.chars[0] == '#')
        {
            if(numOpenColors == 0)
                appendInvalidPlaceholder(buffer, "{", &placeholderValue, i, formatstr->length);
            else
            {
                ffStrbufAppendS(buffer, FASTFETCH_TEXT_MODIFIER_RESET);
                --numOpenColors;
            }

            continue;
        }

        // test for if, if so evaluate it
        if(placeholderValue.chars[0] == '?')
        {
            ffStrbufSubstrAfter(&placeholderValue, 0);

            uint32_t index = getArgumentIndex(&placeholderValue);

            // testing for an invalid index
            if(index > numArgs)
            {
                appendInvalidPlaceholder(buffer, "{?", &placeholderValue, i, formatstr->length);
                continue;
            }

            // continue normally if an format arg is set and the value is > 0
            if(formatArgSet(&arguments[index - 1]))
            {
                ++numOpenIfs;
                continue;
            }

            // fastforward to the end of the if without printing the in between
            i = ffStrbufNextIndexS(formatstr, i, "{?}") + 2; // 2 is the length of "{?}" - 1 because the loop will increment it again directly after continue
            continue;
        }

        // test for not if, if so evaluate it
        if(placeholderValue.chars[0] == '/')
        {
            ffStrbufSubstrAfter(&placeholderValue, 0);

            uint32_t index = getArgumentIndex(&placeholderValue);

            // testing for an invalid index
            if(index > numArgs)
            {
                appendInvalidPlaceholder(buffer, "{/", &placeholderValue, i, formatstr->length);
                continue;
            }

            //continue normally if an format arg is not set or the value is 0
            if(!formatArgSet(&arguments[index - 1]))
            {
                ++numOpenNotIfs;
                continue;
            }

            // fastforward to the end of the if without printing the in between
            i = ffStrbufNextIndexS(formatstr, i, "{/}") + 2; // 2 is the length of "{/}" - 1 because the loop will increment it again directly after continue
            continue;
        }

        //test for color, if so evaluate it
        if(placeholderValue.chars[0] == '#')
        {
            ++numOpenColors;
            ffStrbufSubstrAfter(&placeholderValue, 0);
            ffStrbufAppendS(buffer, "\033[");
            ffStrbufAppend(buffer, &placeholderValue);
            ffStrbufAppendC(buffer, 'm');
            continue;
        }

        uint32_t index = getArgumentIndex(&placeholderValue);

        // test for invalid index
        if(index > numArgs)
        {
            appendInvalidPlaceholder(buffer, "{", &placeholderValue, i, formatstr->length);
            continue;
        }

        ffFormatAppendFormatArg(buffer, &arguments[index - 1]);
    }

    ffStrbufAppendS(buffer, FASTFETCH_TEXT_MODIFIER_RESET);
}
