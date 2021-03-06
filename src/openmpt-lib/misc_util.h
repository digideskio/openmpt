#ifndef MISC_UTIL_H
#define MISC_UTIL_H

#include <cstdint>
#include <sstream>
#include <string>
#include <limits>
#include <type_traits>

#ifndef bad_max
#define bad_max(a,b)  (((a) > (b)) ? (a) : (b))
#endif

#ifndef bad_min
#define bad_min(a,b)  (((a) < (b)) ? (a) : (b))
#endif

//Convert object(typically number) to string
template<class T>
inline std::string Stringify(const T& x)
//--------------------------
{
    std::ostringstream o;
    if(!(o << x)) return "FAILURE";
    else return o.str();
}

//Convert string to number.
template<class T>
inline T ConvertStrTo(LPCSTR psz)
//-------------------------------
{
    static_assert(std::tr1::is_const<T>::value == false
               && std::tr1::is_volatile<T>::value == false,
                  "Const and volatile types are not handled correctly.");
    if (std::numeric_limits<T>::is_integer) {
        return static_cast<T>(atoi(psz));
    } else {
        return static_cast<T>(atof(psz));
    }
}

template<> inline uint32_t ConvertStrTo(LPCSTR psz) {return strtoul(psz, nullptr, 10);}
template<> inline int64_t ConvertStrTo(LPCSTR psz) {return _strtoi64(psz, nullptr, 10);}
template<> inline uint64_t ConvertStrTo(LPCSTR psz) {return _strtoui64(psz, nullptr, 10);}

// Sets last character to null in given char array.
// Size of the array must be known at compile time.
template <size_t size>
inline void SetNullTerminator(char (&buffer)[size])
//-------------------------------------------------
{
    static_assert(size > 0,
        "SetNullTerminator must not be applied to empty buffers");
    buffer[size-1] = 0;
}


// Memset given object to zero.
template <class T>
inline void MemsetZero(T& a)
{
    static_assert(std::tr1::is_pointer<T>::value == false,
        "Won't memset pointers.");
        /*
    static_assert(std::tr1::is_pod<T>::value == true,
        "Won't memset non-pods.");
        */
    memset(&a, 0, sizeof(T));
}


// Limits 'val' to given range. If 'val' is less than 'lowerLimit', 'val' is set to value 'lowerLimit'.
// Similarly if 'val' is greater than 'upperLimit', 'val' is set to value 'upperLimit'.
// If 'lowerLimit' > 'upperLimit', 'val' won't be modified.
template<class T, class C>
inline void Limit(T& val, const C lowerLimit, const C upperLimit)
//---------------------------------------------------------------
{
    if(lowerLimit > upperLimit) return;
    if(val < lowerLimit) val = lowerLimit;
    else if(val > upperLimit) val = upperLimit;
}


// Like Limit, but with upperlimit only.
template<class T, class C>
inline void LimitMax(T& val, const C upperLimit)
//----------------------------------------------
{
    if(val > upperLimit)
            val = upperLimit;
}


// Like Limit, but returns value
#ifndef CLAMP
#define CLAMP(number, low, high) bad_min(high, bad_max(low, number))
#endif


LPCCH LoadResource(LPCTSTR lpName, LPCTSTR lpType, LPCCH& pData, size_t& nSize, HGLOBAL& hglob);
CString GetErrorMessage(uint32_t nErrorCode);


// Sanitize a filename (remove special chars)
template <size_t size>
void SanitizeFilename(char (&buffer)[size])
//-----------------------------------------
{
    static_assert(size > 0,
        "SetNullTerminator must not be applied to empty buffers");
    for(size_t i = 0; i < size; i++)
    {
            if(        buffer[i] == '\\' ||
                    buffer[i] == '\"' ||
                    buffer[i] == '/'  ||
                    buffer[i] == ':'  ||
                    buffer[i] == '?'  ||
                    buffer[i] == '<'  ||
                    buffer[i] == '>'  ||
                    buffer[i] == '*')
            {
                    for(size_t j = i + 1; j < size; j++)
                    {
                            buffer[j - 1] = buffer[j];
                    }
                    buffer[size - 1] = 0;
            }
    }
}


// Convert a 0-terminated string to a space-padded string
template <size_t size>
void NullToSpaceString(char (&buffer)[size])
//------------------------------------------
{
    static_assert(size > 0, "HUUUU:");
    size_t pos = size;
    while (pos-- > 0)
            if (buffer[pos] == 0)
                    buffer[pos] = 32;
    buffer[size - 1] = 0;
}


// Convert a space-padded string to a 0-terminated string
template <size_t size>
void SpaceToNullString(char (&buffer)[size])
//------------------------------------------
{
    static_assert(size > 0, "DERRR");
    // First, remove any Nulls
    NullToSpaceString(buffer);
    size_t pos = size;
    while (pos-- > 0)
    {
            if (buffer[pos] == 32)
                    buffer[pos] = 0;
            else if(buffer[pos] != 0)
                    break;
    }
    buffer[size - 1] = 0;
}


// Remove any chars after the first null char
template <size_t size>
void FixNullString(char (&buffer)[size])
//--------------------------------------
{
    static_assert(size > 0, "HUURURUUR");
    size_t pos = 0;
    // Find the first null char.
    while(buffer[pos] != '\0' && pos < size)
    {
            pos++;
    }
    // Remove everything after the null char.
    while(pos < size)
    {
            buffer[pos++] = '\0';
    }
}


// Convert a space-padded string to a 0-terminated string. STATIC VERSION! (use this if the maximum string length is known)
// Additional template parameter to specifify the bad_max length of the final string,
// not including null char (useful for e.g. mod loaders)
template <size_t length, size_t size>
void SpaceToNullStringFixed(char (&buffer)[size])
//------------------------------------------------
{
    static_assert(size > 0, "HURR");
    static_assert(length < size, "DURR");
    // Remove Nulls in string
    SpaceToNullString(buffer);
    // Overwrite trailing chars
    for(size_t pos = length; pos < size; pos++)
    {
            buffer[pos] = 0;
    }
}


// Convert a space-padded string to a 0-terminated string. DYNAMIC VERSION!
// Additional function parameter to specifify the bad_max length of the final string,
// not including null char (useful for e.g. mod loaders)
template <size_t size>
void SpaceToNullStringFixed(char (&buffer)[size], size_t length)
//--------------------------------------------------------------
{
    static_assert(size > 0, "HERFEN");
    ASSERT(length < size);
    // Remove Nulls in string
    SpaceToNullString(buffer);
    // Overwrite trailing chars
    for(size_t pos = length; pos < size; pos++)
    {
            buffer[pos] = 0;
    }
}

namespace Util
{
    // Like std::max, but avoids conflict with bad_max-macro.
    template <class T> inline const T& Max(const T& a, const T& b) {return (std::max)(a, b);}

    // Returns maximum value of given integer type.
    template <class T> inline T MaxValueOfType(const T&) {
        static_assert(
            std::numeric_limits<T>::is_integer == true,
            "Only interger types are allowed."
        );
        return (std::numeric_limits<T>::max)();
    }


};

namespace Util { namespace sdTime
{
    // Returns string containing date and time ended with newline.
    std::basic_string<TCHAR> GetDateTimeStr();

    time_t MakeGmTime(tm& timeUtc);

}}; // namespace Util::sdTime


#endif
