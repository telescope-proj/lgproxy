
if [ ! -f /usr/bin/protoc-c ]
then
    echo "Protocol Buffers C compiler required."
    echo "Please install it from your distribution's package manager."
    echo "Fedora, RHEL, and derivatives: protobuf-c-devel protobuf-c-compiler"
    echo "Debian, Ubuntu, and derivatives: libprotobuf-c-dev protobuf-c-compiler"
    exit 1
fi

rm -f lp_msg.pb-c.c ../include/lp_msg.pb-c.h
protoc-c --c_out=. lp_msg.proto
sed -i 's,\/\*$,/**,g' lp_msg.pb-c.h
mv lp_msg.pb-c.h ../include/lp_msg.pb-c.h

echo "Regenerated lp_msg.pb-c.c and lp_msg.pb-c.h"