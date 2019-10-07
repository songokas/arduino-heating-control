/**
   \brief Convert an integer to a string.

   The function itoa() converts the integer value from \c val into an
   ASCII representation that will be stored under \c s.  The caller
   is responsible for providing sufficient storage in \c s.

   \note The minimal size of the buffer \c s depends on the choice of
   radix. For example, if the radix is 2 (binary), you need to supply a buffer
   with a minimal length of 8 * sizeof (int) + 1 characters, i.e. one
   character for each bit plus one for the string terminator. Using a larger
   radix will require a smaller minimal buffer size.

   \warning If the buffer is too small, you risk a buffer overflow.

   Conversion is done using the \c radix as base, which may be a
   number between 2 (binary conversion) and up to 36.  If \c radix
   is greater than 10, the next digit after \c '9' will be the letter
   \c 'a'.
    
    If radix is 10 and val is negative, a minus sign will be prepended.

   The itoa() function returns the pointer passed as \c s.
*/
#ifdef  __DOXYGEN__
extern char *itoa(int val, char *s, int radix);
#else
extern __inline__
char *itoa (int __val, char *__s, int __radix)
{
    return __s;
    /*if (!__builtin_constant_p (__radix)) {
	extern char *__itoa (int, char *, int);
	return __itoa (__val, __s, __radix);
    } else if (__radix < 2 || __radix > 36) {
	*__s = 0;
	return __s;
    } else {
	extern char *__itoa_ncheck (int, char *, unsigned char);
	return __itoa_ncheck (__val, __s, __radix);
    }*/
}
#endif

/**
 \ingroup avr_stdlib
 
   \brief Convert a long integer to a string.

   The function ltoa() converts the long integer value from \c val into an
   ASCII representation that will be stored under \c s.  The caller
   is responsible for providing sufficient storage in \c s.

   \note The minimal size of the buffer \c s depends on the choice of
   radix. For example, if the radix is 2 (binary), you need to supply a buffer
   with a minimal length of 8 * sizeof (long int) + 1 characters, i.e. one
   character for each bit plus one for the string terminator. Using a larger
   radix will require a smaller minimal buffer size.

   \warning If the buffer is too small, you risk a buffer overflow.

   Conversion is done using the \c radix as base, which may be a
   number between 2 (binary conversion) and up to 36.  If \c radix
   is greater than 10, the next digit after \c '9' will be the letter
   \c 'a'.

   If radix is 10 and val is negative, a minus sign will be prepended.

   The ltoa() function returns the pointer passed as \c s.
*/
#ifdef  __DOXYGEN__
extern char *ltoa(long val, char *s, int radix);
#else
extern __inline__
char *ltoa (long __val, char *__s, int __radix)
{
    return __s;
    /*if (!__builtin_constant_p (__radix)) {
	extern char *__ltoa (long, char *, int);
	return __ltoa (__val, __s, __radix);
    } else if (__radix < 2 || __radix > 36) {
	*__s = 0;
	return __s;
    } else {
	extern char *__ltoa_ncheck (long, char *, unsigned char);
	return __ltoa_ncheck (__val, __s, __radix);
    }*/
}
#endif

/**
 \ingroup avr_stdlib

   \brief Convert an unsigned integer to a string.

   The function utoa() converts the unsigned integer value from \c val into an
   ASCII representation that will be stored under \c s.  The caller
   is responsible for providing sufficient storage in \c s.

   \note The minimal size of the buffer \c s depends on the choice of
   radix. For example, if the radix is 2 (binary), you need to supply a buffer
   with a minimal length of 8 * sizeof (unsigned int) + 1 characters, i.e. one
   character for each bit plus one for the string terminator. Using a larger
   radix will require a smaller minimal buffer size.

   \warning If the buffer is too small, you risk a buffer overflow.

   Conversion is done using the \c radix as base, which may be a
   number between 2 (binary conversion) and up to 36.  If \c radix
   is greater than 10, the next digit after \c '9' will be the letter
   \c 'a'.

   The utoa() function returns the pointer passed as \c s.
*/
#ifdef  __DOXYGEN__
extern char *utoa(unsigned int val, char *s, int radix);
#else
extern __inline__
char *utoa (unsigned int __val, char *__s, int __radix)
{
    return __s;
    /*if (!__builtin_constant_p (__radix)) {
	extern char *__utoa (unsigned int, char *, int);
	return __utoa (__val, __s, __radix);
    } else if (__radix < 2 || __radix > 36) {
	*__s = 0;
	return __s;
    } else {
	extern char *__utoa_ncheck (unsigned int, char *, unsigned char);
	return __utoa_ncheck (__val, __s, __radix);
    }*/
}
#endif

/**
 \ingroup avr_stdlib
   \brief Convert an unsigned long integer to a string.

   The function ultoa() converts the unsigned long integer value from
   \c val into an ASCII representation that will be stored under \c s.
   The caller is responsible for providing sufficient storage in \c s.

   \note The minimal size of the buffer \c s depends on the choice of
   radix. For example, if the radix is 2 (binary), you need to supply a buffer
   with a minimal length of 8 * sizeof (unsigned long int) + 1 characters,
   i.e. one character for each bit plus one for the string terminator. Using a
   larger radix will require a smaller minimal buffer size.

   \warning If the buffer is too small, you risk a buffer overflow.

   Conversion is done using the \c radix as base, which may be a
   number between 2 (binary conversion) and up to 36.  If \c radix
   is greater than 10, the next digit after \c '9' will be the letter
   \c 'a'.

   The ultoa() function returns the pointer passed as \c s.
*/
#ifdef  __DOXYGEN__
extern char *ultoa(unsigned long val, char *s, int radix);
#else
extern __inline__
char *ultoa (unsigned long __val, char *__s, int __radix)
{
    return __s;
    /*if (!__builtin_constant_p (__radix)) {
	extern char *__ultoa (unsigned long, char *, int);
	return __ultoa (__val, __s, __radix);
    } else if (__radix < 2 || __radix > 36) {
	*__s = 0;
	return __s;
    } else {
	extern char *__ultoa_ncheck (unsigned long, char *, unsigned char);
	return __ultoa_ncheck (__val, __s, __radix);
    }*/
}
#endif

/**
   \ingroup avr_stdlib
   The dtostre() function converts the double value passed in \c val into
   an ASCII representation that will be stored under \c s.  The caller
   is responsible for providing sufficient storage in \c s.

   Conversion is done in the format \c "[-]d.ddde±dd" where there is
   one digit before the decimal-point character and the number of
   digits after it is equal to the precision \c prec; if the precision
   is zero, no decimal-point character appears.  If \c flags has the
   DTOSTRE_UPPERCASE bit set, the letter \c 'E' (rather than \c 'e' ) will be
   used to introduce the exponent.  The exponent always contains two
   digits; if the value is zero, the exponent is \c "00".

   If \c flags has the DTOSTRE_ALWAYS_SIGN bit set, a space character
   will be placed into the leading position for positive numbers.

   If \c flags has the DTOSTRE_PLUS_SIGN bit set, a plus sign will be
   used instead of a space character in this case.

   The dtostre() function returns the pointer to the converted string \c s.
*/
extern __inline__
char *dtostre(double __val, char *__s, unsigned char __prec,
		     unsigned char __flags)
{
    return __s;
}

/**
   \ingroup avr_stdlib
   The dtostrf() function converts the double value passed in \c val into
   an ASCII representationthat will be stored under \c s.  The caller
   is responsible for providing sufficient storage in \c s.

   Conversion is done in the format \c "[-]d.ddd".  The minimum field
   width of the output string (including the possible \c '.' and the possible
   sign for negative values) is given in \c width, and \c prec determines
   the number of digits after the decimal sign. \c width is signed value,
   negative for left adjustment.

   The dtostrf() function returns the pointer to the converted string \c s.
*/
extern __inline__
char *dtostrf(double __val, signed char __width,
                     unsigned char __prec, char *__s)
{
    return __s;
}


