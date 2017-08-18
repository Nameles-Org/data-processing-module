FROM apastor/debian-zmqpp:stretch

#ENV RCV_PORT 58510

RUN apt-get install -y postgresql postgresql-server-dev-all libpq-dev libpqxx-dev
RUN pg_lsclusters | tail +2 | cut -f1,2 -d' ' | while read x; do pg_dropcluster --stop $x; done

RUN apt-get install -y libgflags-dev libboost-system-dev libboost-thread-dev
RUN apt-get install -y sudo bc python3-psycopg2

WORKDIR /Nameles-streaming

ADD . /Nameles-streaming

RUN make -C ./src/pgsql
RUN make

#EXPOSE $RCV_PORT

ENTRYPOINT sh /Nameles-streaming/src/pgsql_config.sh && /Nameles-streaming/data-processing \
    -rcvport $RCV_PORT -notify_sockets $NOTIFY_SOCKETS -dbIP $DB_IP \
    -dbUSER $DB_USER -dbPWD $DB_PWD -dbNAME $DB_NAME -nWorkers $NWORKERS \
    -max_msgs $MAX_MSGS -python_script_name $PYTHON_SCRIPT_NAME
