#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = nanomsg

CONFIG(MINGW32):LIBNANOMSGSRC = "C:\softs\nanomsg-0.8-beta"
CONFIG(MINGW64):LIBNANOMSGSRC = "C:\softs\nanomsg-0.8-beta"

CONFIG(MINGW32):DEFINES += NN_HAVE_WINDOWS=1
CONFIG(MINGW32):DEFINES += _CRT_SECURE_NO_WARNINGS=1
CONFIG(MINGW32):DEFINES += NN_HAVE_MINGW=1
CONFIG(MINGW32):DEFINES += NN_HAVE_STDINT=1
CONFIG(MINGW32):DEFINES += _WIN32_WINNT=0x0600
CONFIG(MINGW32):DEFINES += NN_EXPORTS=1

CONFIG(MINGW64):DEFINES += NN_HAVE_WINDOWS=1
CONFIG(MINGW64):DEFINES += _CRT_SECURE_NO_WARNINGS=1
CONFIG(MINGW64):DEFINES += NN_HAVE_MINGW=1
CONFIG(MINGW64):DEFINES += NN_HAVE_STDINT=1
CONFIG(MINGW64):DEFINES += _WIN32_WINNT=0x0600
CONFIG(MINGW64):DEFINES += NN_EXPORTS=1
CONFIG(MINGW64):DEFINES += _POSIX_C_SOURCE=1

INCLUDEPATH += $$LIBNANOMSGSRC/src

SOURCES = $$LIBNANOMSGSRC/src/core/ep.c\
$$LIBNANOMSGSRC/src/core/epbase.c\
$$LIBNANOMSGSRC/src/core/global.c\
$$LIBNANOMSGSRC/src/core/pipe.c\
$$LIBNANOMSGSRC/src/core/poll.c\
$$LIBNANOMSGSRC/src/core/sock.c\
$$LIBNANOMSGSRC/src/core/sockbase.c\
$$LIBNANOMSGSRC/src/core/symbol.c\
$$LIBNANOMSGSRC/src/aio/ctx.c\
$$LIBNANOMSGSRC/src/aio/fsm.c\
$$LIBNANOMSGSRC/src/aio/poller.c\
$$LIBNANOMSGSRC/src/aio/pool.c\
$$LIBNANOMSGSRC/src/aio/timer.c\
$$LIBNANOMSGSRC/src/aio/timerset.c\
$$LIBNANOMSGSRC/src/aio/usock.c\
$$LIBNANOMSGSRC/src/aio/worker.c\
$$LIBNANOMSGSRC/src/utils/alloc.c\
$$LIBNANOMSGSRC/src/utils/atomic.c\
$$LIBNANOMSGSRC/src/utils/chunk.c\
$$LIBNANOMSGSRC/src/utils/chunkref.c\
$$LIBNANOMSGSRC/src/utils/clock.c\
$$LIBNANOMSGSRC/src/utils/closefd.c\
$$LIBNANOMSGSRC/src/utils/efd.c\
$$LIBNANOMSGSRC/src/utils/err.c\
$$LIBNANOMSGSRC/src/utils/glock.c\
$$LIBNANOMSGSRC/src/utils/hash.c\
$$LIBNANOMSGSRC/src/utils/list.c\
$$LIBNANOMSGSRC/src/utils/msg.c\
$$LIBNANOMSGSRC/src/utils/mutex.c\
$$LIBNANOMSGSRC/src/utils/queue.c\
$$LIBNANOMSGSRC/src/utils/random.c\
$$LIBNANOMSGSRC/src/utils/sem.c\
$$LIBNANOMSGSRC/src/utils/sleep.c\
$$LIBNANOMSGSRC/src/utils/thread.c\
$$LIBNANOMSGSRC/src/utils/wire.c\
$$LIBNANOMSGSRC/src/devices/device.c\
$$LIBNANOMSGSRC/src/devices/tcpmuxd.c\
$$LIBNANOMSGSRC/src/protocols/utils/dist.c\
$$LIBNANOMSGSRC/src/protocols/utils/excl.c\
$$LIBNANOMSGSRC/src/protocols/utils/fq.c\
$$LIBNANOMSGSRC/src/protocols/utils/lb.c\
$$LIBNANOMSGSRC/src/protocols/utils/priolist.c\
$$LIBNANOMSGSRC/src/protocols/bus/bus.c\
$$LIBNANOMSGSRC/src/protocols/bus/xbus.c\
$$LIBNANOMSGSRC/src/protocols/pipeline/push.c\
$$LIBNANOMSGSRC/src/protocols/pipeline/pull.c\
$$LIBNANOMSGSRC/src/protocols/pipeline/xpull.c\
$$LIBNANOMSGSRC/src/protocols/pipeline/xpush.c\
$$LIBNANOMSGSRC/src/protocols/pair/pair.c\
$$LIBNANOMSGSRC/src/protocols/pair/xpair.c\
$$LIBNANOMSGSRC/src/protocols/pubsub/pub.c\
$$LIBNANOMSGSRC/src/protocols/pubsub/sub.c\
$$LIBNANOMSGSRC/src/protocols/pubsub/trie.c\
$$LIBNANOMSGSRC/src/protocols/pubsub/xpub.c\
$$LIBNANOMSGSRC/src/protocols/pubsub/xsub.c\
$$LIBNANOMSGSRC/src/protocols/reqrep/req.c\
$$LIBNANOMSGSRC/src/protocols/reqrep/rep.c\
$$LIBNANOMSGSRC/src/protocols/reqrep/task.c\
$$LIBNANOMSGSRC/src/protocols/reqrep/xrep.c\
$$LIBNANOMSGSRC/src/protocols/reqrep/xreq.c\
$$LIBNANOMSGSRC/src/protocols/survey/respondent.c\
$$LIBNANOMSGSRC/src/protocols/survey/surveyor.c\
$$LIBNANOMSGSRC/src/protocols/survey/xrespondent.c\
$$LIBNANOMSGSRC/src/protocols/survey/xsurveyor.c\
$$LIBNANOMSGSRC/src/transports/utils/backoff.c\
$$LIBNANOMSGSRC/src/transports/utils/dns.c\
$$LIBNANOMSGSRC/src/transports/utils/iface.c\
$$LIBNANOMSGSRC/src/transports/utils/literal.c\
$$LIBNANOMSGSRC/src/transports/utils/port.c\
$$LIBNANOMSGSRC/src/transports/utils/streamhdr.c\
$$LIBNANOMSGSRC/src/transports/utils/base64.c\
$$LIBNANOMSGSRC/src/transports/inproc/binproc.c\
$$LIBNANOMSGSRC/src/transports/inproc/cinproc.c\
$$LIBNANOMSGSRC/src/transports/inproc/inproc.c\
$$LIBNANOMSGSRC/src/transports/inproc/ins.c\
$$LIBNANOMSGSRC/src/transports/inproc/msgqueue.c\
$$LIBNANOMSGSRC/src/transports/inproc/sinproc.c\
$$LIBNANOMSGSRC/src/transports/ipc/aipc.c\
$$LIBNANOMSGSRC/src/transports/ipc/bipc.c\
$$LIBNANOMSGSRC/src/transports/ipc/cipc.c\
$$LIBNANOMSGSRC/src/transports/ipc/ipc.c\
$$LIBNANOMSGSRC/src/transports/ipc/sipc.c\
$$LIBNANOMSGSRC/src/transports/tcp/atcp.c\
$$LIBNANOMSGSRC/src/transports/tcp/btcp.c\
$$LIBNANOMSGSRC/src/transports/tcp/ctcp.c\
$$LIBNANOMSGSRC/src/transports/tcp/stcp.c\
$$LIBNANOMSGSRC/src/transports/tcp/tcp.c\
$$LIBNANOMSGSRC/src/transports/tcpmux/atcpmux.c\
$$LIBNANOMSGSRC/src/transports/tcpmux/btcpmux.c\
$$LIBNANOMSGSRC/src/transports/tcpmux/ctcpmux.c\
$$LIBNANOMSGSRC/src/transports/tcpmux/stcpmux.c\
$$LIBNANOMSGSRC/src/transports/tcpmux/tcpmux.c\
$$LIBNANOMSGSRC/src/transports/ws/aws.c\
$$LIBNANOMSGSRC/src/transports/ws/bws.c\
$$LIBNANOMSGSRC/src/transports/ws/cws.c\
$$LIBNANOMSGSRC/src/transports/ws/sws.c\
$$LIBNANOMSGSRC/src/transports/ws/ws.c\
$$LIBNANOMSGSRC/src/transports/ws/ws_handshake.c\
$$LIBNANOMSGSRC/src/transports/ws/sha1.c

HEADERS = $$LIBNANOMSGSRC/src/nn.h\
$$LIBNANOMSGSRC/src/inproc.h\
$$LIBNANOMSGSRC/src/ipc.h\
$$LIBNANOMSGSRC/src/tcp.h\
$$LIBNANOMSGSRC/src/ws.h\
$$LIBNANOMSGSRC/src/pair.h\
$$LIBNANOMSGSRC/src/pubsub.h\
$$LIBNANOMSGSRC/src/reqrep.h\
$$LIBNANOMSGSRC/src/pipeline.h\
$$LIBNANOMSGSRC/src/survey.h\
$$LIBNANOMSGSRC/src/bus.h\
$$LIBNANOMSGSRC/src/core/ep.h\
$$LIBNANOMSGSRC/src/core/global.h\
$$LIBNANOMSGSRC/src/core/sock.h\
$$LIBNANOMSGSRC/src/aio/ctx.h\
$$LIBNANOMSGSRC/src/aio/fsm.h\
$$LIBNANOMSGSRC/src/aio/poller.h\
$$LIBNANOMSGSRC/src/aio/poller_epoll.h\
$$LIBNANOMSGSRC/src/aio/poller_kqueue.h\
$$LIBNANOMSGSRC/src/aio/poller_poll.h\
$$LIBNANOMSGSRC/src/aio/pool.h\
$$LIBNANOMSGSRC/src/aio/timer.h\
$$LIBNANOMSGSRC/src/aio/timerset.h\
$$LIBNANOMSGSRC/src/aio/usock.h\
$$LIBNANOMSGSRC/src/aio/usock_posix.h\
$$LIBNANOMSGSRC/src/aio/usock_win.h\
$$LIBNANOMSGSRC/src/aio/worker.h\
$$LIBNANOMSGSRC/src/aio/worker_posix.h\
$$LIBNANOMSGSRC/src/aio/worker_win.h\
$$LIBNANOMSGSRC/src/utils/alloc.h\
$$LIBNANOMSGSRC/src/utils/atomic.h\
$$LIBNANOMSGSRC/src/utils/attr.h\
$$LIBNANOMSGSRC/src/utils/chunk.h\
$$LIBNANOMSGSRC/src/utils/chunkref.h\
$$LIBNANOMSGSRC/src/utils/clock.h\
$$LIBNANOMSGSRC/src/utils/closefd.h\
$$LIBNANOMSGSRC/src/utils/cont.h\
$$LIBNANOMSGSRC/src/utils/efd.h\
$$LIBNANOMSGSRC/src/utils/efd_eventfd.h\
$$LIBNANOMSGSRC/src/utils/efd_pipe.h\
$$LIBNANOMSGSRC/src/utils/efd_socketpair.h\
$$LIBNANOMSGSRC/src/utils/efd_win.h\
$$LIBNANOMSGSRC/src/utils/err.h\
$$LIBNANOMSGSRC/src/utils/fast.h\
$$LIBNANOMSGSRC/src/utils/fd.h\
$$LIBNANOMSGSRC/src/utils/glock.h\
$$LIBNANOMSGSRC/src/utils/hash.h\
$$LIBNANOMSGSRC/src/utils/int.h\
$$LIBNANOMSGSRC/src/utils/list.h\
$$LIBNANOMSGSRC/src/utils/msg.h\
$$LIBNANOMSGSRC/src/utils/mutex.h\
$$LIBNANOMSGSRC/src/utils/queue.h\
$$LIBNANOMSGSRC/src/utils/random.h\
$$LIBNANOMSGSRC/src/utils/sem.h\
$$LIBNANOMSGSRC/src/utils/sleep.h\
$$LIBNANOMSGSRC/src/utils/thread.h\
$$LIBNANOMSGSRC/src/utils/thread_posix.h\
$$LIBNANOMSGSRC/src/utils/thread_win.h\
$$LIBNANOMSGSRC/src/utils/wire.h\
$$LIBNANOMSGSRC/src/devices/device.h\
$$LIBNANOMSGSRC/src/protocols/utils/dist.h\
$$LIBNANOMSGSRC/src/protocols/utils/excl.h\
$$LIBNANOMSGSRC/src/protocols/utils/fq.h\
$$LIBNANOMSGSRC/src/protocols/utils/lb.h\
$$LIBNANOMSGSRC/src/protocols/utils/priolist.h\
$$LIBNANOMSGSRC/src/protocols/bus/bus.h\
$$LIBNANOMSGSRC/src/protocols/bus/xbus.h\
$$LIBNANOMSGSRC/src/protocols/pipeline/push.h\
$$LIBNANOMSGSRC/src/protocols/pipeline/pull.h\
$$LIBNANOMSGSRC/src/protocols/pipeline/xpull.h\
$$LIBNANOMSGSRC/src/protocols/pipeline/xpush.h\
$$LIBNANOMSGSRC/src/protocols/pair/pair.h\
$$LIBNANOMSGSRC/src/protocols/pair/xpair.h\
$$LIBNANOMSGSRC/src/protocols/pubsub/pub.h\
$$LIBNANOMSGSRC/src/protocols/pubsub/sub.h\
$$LIBNANOMSGSRC/src/protocols/pubsub/trie.h\
$$LIBNANOMSGSRC/src/protocols/pubsub/xpub.h\
$$LIBNANOMSGSRC/src/protocols/pubsub/xsub.h\
$$LIBNANOMSGSRC/src/protocols/reqrep/req.h\
$$LIBNANOMSGSRC/src/protocols/reqrep/rep.h\
$$LIBNANOMSGSRC/src/protocols/reqrep/task.h\
$$LIBNANOMSGSRC/src/protocols/reqrep/xrep.h\
$$LIBNANOMSGSRC/src/protocols/reqrep/xreq.h\
$$LIBNANOMSGSRC/src/protocols/survey/respondent.h\
$$LIBNANOMSGSRC/src/protocols/survey/surveyor.h\
$$LIBNANOMSGSRC/src/protocols/survey/xrespondent.h\
$$LIBNANOMSGSRC/src/protocols/survey/xsurveyor.h\
$$LIBNANOMSGSRC/src/transports/utils/backoff.h\
$$LIBNANOMSGSRC/src/transports/utils/dns.h\
$$LIBNANOMSGSRC/src/transports/utils/dns_getaddrinfo.h\
$$LIBNANOMSGSRC/src/transports/utils/dns_getaddrinfo_a.h\
$$LIBNANOMSGSRC/src/transports/utils/iface.h\
$$LIBNANOMSGSRC/src/transports/utils/literal.h\
$$LIBNANOMSGSRC/src/transports/utils/port.h\
$$LIBNANOMSGSRC/src/transports/utils/streamhdr.h\
$$LIBNANOMSGSRC/src/transports/utils/base64.h\
$$LIBNANOMSGSRC/src/transports/inproc/binproc.h\
$$LIBNANOMSGSRC/src/transports/inproc/cinproc.h\
$$LIBNANOMSGSRC/src/transports/inproc/inproc.h\
$$LIBNANOMSGSRC/src/transports/inproc/ins.h\
$$LIBNANOMSGSRC/src/transports/inproc/msgqueue.h\
$$LIBNANOMSGSRC/src/transports/inproc/sinproc.h\
$$LIBNANOMSGSRC/src/transports/ipc/aipc.h\
$$LIBNANOMSGSRC/src/transports/ipc/bipc.h\
$$LIBNANOMSGSRC/src/transports/ipc/cipc.h\
$$LIBNANOMSGSRC/src/transports/ipc/ipc.h\
$$LIBNANOMSGSRC/src/transports/ipc/sipc.h\
$$LIBNANOMSGSRC/src/transports/tcp/atcp.h\
$$LIBNANOMSGSRC/src/transports/tcp/btcp.h\
$$LIBNANOMSGSRC/src/transports/tcp/ctcp.h\
$$LIBNANOMSGSRC/src/transports/tcp/stcp.h\
$$LIBNANOMSGSRC/src/transports/tcp/tcp.h\
$$LIBNANOMSGSRC/src/transports/tcpmux/atcpmux.h\
$$LIBNANOMSGSRC/src/transports/tcpmux/btcpmux.h\
$$LIBNANOMSGSRC/src/transports/tcpmux/ctcpmux.h\
$$LIBNANOMSGSRC/src/transports/tcpmux/stcpmux.h\
$$LIBNANOMSGSRC/src/transports/tcpmux/tcpmux.h\
$$LIBNANOMSGSRC/src/transports/ws/aws.h\
$$LIBNANOMSGSRC/src/transports/ws/bws.h\
$$LIBNANOMSGSRC/src/transports/ws/cws.h\
$$LIBNANOMSGSRC/src/transports/ws/sws.h\
$$LIBNANOMSGSRC/src/transports/ws/ws.h\
$$LIBNANOMSGSRC/src/transports/ws/ws_handshake.h\
$$LIBNANOMSGSRC/src/transports/ws/sha1.h\
$$LIBNANOMSGSRC/src/utils/win.h\
$$LIBNANOMSGSRC/src/aio/poller_epoll.inc\
$$LIBNANOMSGSRC/src/aio/poller_kqueue.inc\
$$LIBNANOMSGSRC/src/aio/poller_poll.inc\
$$LIBNANOMSGSRC/src/aio/usock_posix.inc\
$$LIBNANOMSGSRC/src/aio/usock_win.inc\
$$LIBNANOMSGSRC/src/aio/worker_posix.inc\
$$LIBNANOMSGSRC/src/aio/worker_win.inc\
$$LIBNANOMSGSRC/src/utils/efd_eventfd.inc\
$$LIBNANOMSGSRC/src/utils/efd_pipe.inc\
$$LIBNANOMSGSRC/src/utils/efd_socketpair.inc\
$$LIBNANOMSGSRC/src/utils/efd_win.inc\
$$LIBNANOMSGSRC/src/utils/thread_posix.inc\
$$LIBNANOMSGSRC/src/utils/thread_win.inc\
$$LIBNANOMSGSRC/src/transports/utils/dns_getaddrinfo.inc\
$$LIBNANOMSGSRC/src/transports/utils/dns_getaddrinfo_a.inc

#CONFIG(MINGW32):LIBS += -lws2_32 -lmswsock -ladvapi32
CONFIG(MINGW32):LIBS += -lws2_32 -lmswsock
CONFIG(MINGW64):LIBS += -lws2_32 -lmswsock
