# Nameless-streaming



## Running nameles with docker-compose

### The complete system in the same compose

sudo docker-compose -f nameles-docker-compose.yml up --force-recreate

## Each container from different compose files

1. Create the nameles-net network:
  ```bash
  sudo docker network create --subnet 172.20.0.0/24 --gateway 172.20.0.1 nameles-net
  ```

2. Run docker-compose for the three nameles modules in the following order:
  1. nameles-streaming
  2. scoring-module
  3. dsp-emulator

  ```bash
  sudo docker-compose -f <module-path>/docker-compose.yml up --force-recreate
```

3. Check the proper working of the system accessing to the database from the host

  ```bash
  psql -h 127.0.0.1 -p 5430 -U nameles
  ```

  Note that you need to have installed the postgreSQL client.
  In Debian/Ubuntu systems you need the postgresql-client package:
  ```bash
  sudo apt-get install postgresql-client
  ```
