#!/bin/sh

set -e

NAMELES_DIR=$(cd "$(dirname "$(readlink -f $0)")/../" && pwd)
PGSQL_PORT=5430
PGSQL_MAJOR=$( ls /usr/lib/postgresql/ | sort -r | head -1 )
pg_createcluster --port $PGSQL_PORT --start $PGSQL_MAJOR nmls -- --auth-local=trust \
	--auth-host=trust # WARNING: not secure

echo $PGSQL_PORT > $NAMELES_DIR/nameles-port
MEMTOTAL=$(awk '/^MemTotal:/ {mem_kb=$2}; END {print mem_kb/1024/1024}' /proc/meminfo)
CPU_COUNT=$(lscpu | awk '/^Core\(s\) per socket:/ {cores=$NF}; /^Socket\(s\):/ {sockets=$NF}; END{print cores*sockets}')
AUTOVACUUM_WORKERS=$(( $CPU_COUNT > 5 ? $CPU_COUNT/3 : 1 ))
WORKERS=$(( $CPU_COUNT > 3 ? $CPU_COUNT-$AUTOVACUUM_WORKERS : $CPU_COUNT ))
MEM_BUFFERS=$(echo "($MEMTOTAL*0.125+0.5)/1" | bc)
BUFFERS=$(( $MEM_BUFFERS > 1 ? $MEM_BUFFERS : 1 ))
MEM_WORKMEM=$(echo "$MEM_BUFFERS*2" | bc)
WORKMEM=$(( $MEM_WORKMEM > 1 ? $MEM_WORKMEM : 1 ))
perl -pi -e "s/(?<=^)#?listen_addresses =?.+?(?=\$|\t)/listen_addresses = '*'/; \
    s/(?<=^)#?shared_buffers =?.+?(?=\$|\t)/shared_buffers = ${BUFFERS}GB/; \
    s/(?<=^)#?temp_buffers =?.+?(?=\$|\t)/temp_buffers = ${BUFFERS}GB/; \
    s/(?<=^)#?work_mem =?.+?(?=\$|\t)/work_mem = ${WORKMEM}GB/; \
    s/(?<=^)#?max_stack_depth =?.+?(?=\$|\t)/max_stack_depth = 7680kB/; \
    s/(?<=^)#?checkpoint_segments =?.+?(?=\$|\t)/checkpoint_segments =256/; \
    s/(?<=^)#?checkpoint_timeout =?.+?(?=\$|\t)/checkpoint_timeout = 30min/; \
    s/(?<=^)#?checkpoint_completion_target =?.+?(?=\$|\t)/checkpoint_completion_target = 0.1/; \
    s/(?<=^)#?checkpoint_warning =?.+?(?=\$|\t)/checkpoint_warning = 30s/; \
    s/(?<=^)#?max_worker_processes =?.+?(?=\$|\t)/max_worker_processes = $WORKERS/; \
    s/(?<=^)#?autovacuum_max_workers =?.+?(?=\$|\t)/autovacuum_max_workers = $AUTOVACUUM_WORKERS/" \
    /etc/postgresql/$PGSQL_MAJOR/nmls/postgresql.conf


cp -v $NAMELES_DIR/src/pgsql/libentropy.so /usr/lib/postgresql/$PGSQL_MAJOR/lib
pg_ctlcluster $PGSQL_MAJOR nmls restart
sudo -u postgres psql -p $PGSQL_PORT -f $NAMELES_DIR/src/pgsql/create_db.sql
sudo -u postgres psql  -p $PGSQL_PORT -d nameles -f $NAMELES_DIR/src/pgsql/create_functions.sql
