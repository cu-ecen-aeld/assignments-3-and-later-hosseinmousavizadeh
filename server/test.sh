set -e

rm /var/tmp/aesdsocketdata
make clean
make aesdsocket
./aesdsocket && echo "✅ Success" || echo "❌ Failed with code $?"

