#ifndef FCDLIB_EXPORT_H_
#define FCDLIB_EXPORT_H_

// cmake's WINDOWS_EXPORT_ALL_SYMBOLS only supports functions, so we need dllexport/import for global data
#ifdef _MSC_VER
#ifdef FCDLIB_EXPORTS
#define FCDLIB_API __declspec(dllexport)
#else
#define FCDLIB_API __declspec(dllimport)
#endif
#else
#define FCDLIB_API
#endif

#endif /* FCDLIB_EXPORT_H_ */
