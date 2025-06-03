#! /bin/sh

case "$1" in
    start)
        echo "Starting aesdsocket"
        start-stop-daemon -S -n aesdsocket --exec /usr/bin/aesdsocket -a aesdsocket -- -d
        ;;
    stop)
        echo "Stopping aesdsocket"
        start-stop-daemon -S -n aesdsocket
        ;;
    restart)
        echo "Restarting aesdsocket"
        start-stop-daemon -S -n aesdsocket
        start-stop-daemon -S -n aesdsocket --exec /usr/bin/aesdsocket -a aesdsocket -- -d
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
esac   

exit 0