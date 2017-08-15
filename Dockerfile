FROM apastor/debian-zmqpp

#ENV RCV_PORT 58510

WORKDIR /Nameles-streaming

ADD . /Nameles-streaming

RUN apt-get install -y postgresql-9.4 postgresql-server-dev-all libpq-dev libpqxx-dev
RUN apt-get install -y libgflags-dev libboost-system-dev libboost-thread-dev
RUN apt-get install -y python3-psycopg2 python3-pip
RUN pip3 install pyzmq

RUN make

#EXPOSE $RCV_PORT

ENTRYPOINT /Nameles-streaming/data-processing -rcvport $RCV_PORT -notify_sockets $NOTIFY_SOCKETS \
    -dbIP $DB_IP -dbUSER $DB_USER -dbPWD $DB_PWD -dbNAME $DB_NAME -nWorkers $NWORKERS \
    -max_msgs $MAX_MSGS -python_script_name $PYTHON_SCRIPT_NAME
