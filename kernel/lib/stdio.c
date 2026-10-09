
#define NULL ((void *)0)

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
char *kitoa(int32_t value, char *buf, uint32_t base) {
  static const char digits[] = "0123456789abcdef";

  if (buf == NULL || base < 2 || base > 16) {
    return NULL;
  }

  char *p = buf;
  uint32_t magnitude;

  if (value < 0 && base == 10) {
    *p++ = '-';
    magnitude = (uint32_t)(-(value + 1)) + 1;
  } else {
    magnitude = (uint32_t)value;
  }

  char *start = p;

  do {
    *p++ = digits[magnitude % base];
    magnitude /= base;
  } while (magnitude != 0);

  *p = '\0';

  /* reverse */
  char *left = start;
  char *right = p - 1;

  while (left < right) {
    char tmp = *left;
    *left++ = *right;
    *right-- = tmp;
  }

  return buf;
}

/*
 * Append one character to the output buffer.
 *
 * `pos` always represents the number of characters that would have
 * been written, excluding the terminating '\0'.
 */
static void ksnprintf_putc(char *buf, size_t size, size_t *pos, char c) {
  if (*pos + 1 < size)
    buf[*pos] = c;

  (*pos)++;
}

/*
 * Append a string to the output buffer.
 */
static void ksnprintf_puts(char *buf, size_t size, size_t *pos,
                           const char *str) {
  if (!str)
    str = "(null)";

  while (*str)
    ksnprintf_putc(buf, size, pos, *str++);
}

/*
 * Append an unsigned integer in the specified base.
 */
static void ksnprintf_put_uint(char *buf, size_t size, size_t *pos,
                               uint32_t value, unsigned int base,
                               int uppercase) {
  static const char digits_lower[] = "0123456789abcdef";
  static const char digits_upper[] = "0123456789ABCDEF";

  const char *digits = uppercase ? digits_upper : digits_lower;
  char tmp[32];
  size_t i = 0;

  if (value == 0) {
    ksnprintf_putc(buf, size, pos, '0');
    return;
  }

  while (value != 0) {
    tmp[i++] = digits[value % base];
    value /= base;
  }

  while (i > 0)
    ksnprintf_putc(buf, size, pos, tmp[--i]);
}

/*
 * ksnprintf - Format a string into a bounded buffer.
 *
 * Supported:
 *   %s   string
 *   %c   character
 *   %d   signed decimal integer
 *   %u   unsigned decimal integer
 *   %x   hexadecimal
 *   %X   uppercase hexadecimal
 *   %%   literal '%'
 *
 * Return value:
 *   Number of characters that would have been written, excluding '\0'.
 *
 * The output is always NUL-terminated when size > 0.
 */
int ksnprintf(char *buf, size_t size, const char *fmt, ...) {
  va_list ap;
  size_t pos = 0;

  if (!buf && size != 0)
    return -1;

  if (!fmt)
    return -1;

  va_start(ap, fmt);

  while (*fmt) {
    if (*fmt != '%') {
      ksnprintf_putc(buf, size, &pos, *fmt++);
      continue;
    }

    fmt++;

    if (*fmt == '\0')
      break;

    switch (*fmt) {
    case '%':
      ksnprintf_putc(buf, size, &pos, '%');
      break;

    case 'c':
      ksnprintf_putc(buf, size, &pos, (char)va_arg(ap, int));
      break;

    case 's':
      ksnprintf_puts(buf, size, &pos, va_arg(ap, const char *));
      break;

    case 'd': {
      int32_t value = va_arg(ap, int32_t);

      if (value < 0) {
        ksnprintf_putc(buf, size, &pos, '-');

        /*
         * Avoid signed overflow for INT32_MIN.
         */
        uint32_t magnitude = (uint32_t)(-(value + 1)) + 1;

        ksnprintf_put_uint(buf, size, &pos, magnitude, 10, 0);
      } else {
        ksnprintf_put_uint(buf, size, &pos, (uint32_t)value, 10, 0);
      }

      break;
    }

    case 'u':
      ksnprintf_put_uint(buf, size, &pos, va_arg(ap, uint32_t), 10, 0);
      break;

    case 'x':
      ksnprintf_put_uint(buf, size, &pos, va_arg(ap, uint32_t), 16, 0);
      break;

    case 'X':
      ksnprintf_put_uint(buf, size, &pos, va_arg(ap, uint32_t), 16, 1);
      break;

    default:
      /*
       * Unknown format specifier.
       * Keep it visible instead of silently dropping it.
       */
      ksnprintf_putc(buf, size, &pos, '%');
      ksnprintf_putc(buf, size, &pos, *fmt);
      break;
    }

    fmt++;
  }

  va_end(ap);

  if (size != 0) {
    if (pos < size)
      buf[pos] = '\0';
    else
      buf[size - 1] = '\0';
  }

  return (int)pos;
}
