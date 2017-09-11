#ifndef IIO_CONFIG_H
#define IIO_CONFIG_H

#define LIBIIO_VERSION_MAJOR   0
#define LIBIIO_VERSION_MINOR   10
#define LIBIIO_VERSION_GIT     "3b288d4"

#define LOG_LEVEL Info_L

/* #undef WITH_LOCAL_BACKEND */
#define WITH_XML_BACKEND
/* #undef WITH_NETWORK_BACKEND */
#define WITH_USB_BACKEND
/* #undef WITH_SERIAL_BACKEND */
/* #undef WITH_MATLAB_BINDINGS_API */

/* #undef WITH_NETWORK_GET_BUFFER */
/* #undef WITH_NETWORK_EVENTFD */
/* #undef WITH_IIOD_USBD */
/* #undef WITH_LOCAL_CONFIG */
#define HAS_PIPE2
#define HAS_STRDUP
#define HAS_STRERROR_R
#define HAS_NEWLOCALE
/* #undef HAS_PTHREAD_SETNAME_NP */
/* #undef HAVE_IPV6 */
/* #undef HAVE_AVAHI */
/* #undef NO_THREADS */

#endif /* IIO_CONFIG_H */
