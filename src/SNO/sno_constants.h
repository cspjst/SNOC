/* sno_constants.h — Character sets for SNO pattern functions */

#ifndef SNO_CONSTANTS_H
#define SNO_CONSTANTS_H

/**
 * @file sno_constants.h
 * @brief Predefined character sets for sno_any, sno_span, sno_break, sno_notany
 *
 * These string literals follow SNOBOL4 naming conventions (LETTERS, DIGITS, etc.)
 * with a SNO_ prefix for C namespace safety. All are immutable string literals—
 * zero runtime cost, usable directly as function arguments.
 */

/** Uppercase and lowercase ASCII letters (A-Z, a-z) */
#define SNO_LETTERS        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"

/** Decimal digits (0-9) */
#define SNO_DIGITS         "0123456789"

/** Alphanumeric characters (letters + digits) */
#define SNO_ALNUM          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"

/** Alphanumeric + underscore (C identifier characters) */
#define SNO_ALNUM_U        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_"

/** Whitespace characters (space, tab, CR, LF) */
#define SNO_WHITESPACE     " \t\r\n"

/** SNOBOL operator symbols from greenbook example */
#define SNO_OPSYMS         "+-*/.$&a?#%!"

/** Common punctuation delimiters */
#define SNO_PUNCTUATION    ".,;:!?\"'()[]{}"

/** Hexadecimal digits (0-9, A-F, a-f) */
#define SNO_HEX_DIGITS     "0123456789ABCDEFabcdef"

#endif
