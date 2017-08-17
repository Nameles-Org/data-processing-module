#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright 2017 Antonio Pastor anpastor{at}it.uc3m.es
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""Module with functions to compute the confidence scores of a given day."""

import re
import sys
import zmq
import psycopg2
import argparse
import datetime
from time import time
# from getpass import getuser
from datetime import timedelta
from multiprocessing import Process
from zmq.decorators import context, socket

__author__ = "Antonio Pastor"
__copyright__ = "Copyright 2017, Antonio Pastor"

__license__ = "ALv2"
__version__ = "1.0"
__maintainer__ = "Antonio Pastor"
__email__ = "anpastor@it.uc3m.es"
__status__ = "Development"


POSTGRES_USER = 'nameles'  # getuser()
POSTGRES_DB = 'nameles'
POSTGRES_PORT = 5430
# with open('/usr/share/nameles/nameles-port') as port_file:
#     POSTGRES_PORT = port_file.readline().rstrip()


def main(args):
    """Main function of the module."""
    log_day = args.date
    temp_table = "tuples.temp_" + log_day
    tuples_table = "tuples.ip_ref_" + log_day

    conn = psycopg2.connect("dbname=%s user=%s port=%s"
                            % (POSTGRES_DB, POSTGRES_USER, POSTGRES_PORT))
    cur = conn.cursor()
    cur.execute("SET ROLE nameles")
    conn.commit()
    cur.execute("CREATE table IF NOT EXISTS {} ( ) INHERITS (tuples.ip_ref)"
                .format(temp_table))
    conn.commit()

    start_time = time()
    cur.execute("ANALYZE {}".format(temp_table))
    conn.commit()

    analyze_time = time()
    print("Analyzed temporary table from copy in {}".format(
                            timedelta(seconds=analyze_time-start_time)))

    print("Creating table with (ip, referrer, count) tuples")
    # cur.execute("show work_mem")
    # wm = cur.fetchall()[0][0]
    # cur.execute("SET work_mem = '24GB'")
    conn.commit()
    cur.execute(("CREATE table {0} AS (SELECT ip, referrer, sum(cnt) AS"
                " cnt FROM {1} GROUP BY ip,referrer)")
                .format(tuples_table, temp_table))
    # cur.execute("SET work_mem = '{0}'".format(wm))
    cur.execute("DROP TABLE {0}".format(temp_table))
    conn.commit()
    conn.close()

    tuples_time = time()
    print("Created tuples table in {}".format(
            timedelta(seconds=tuples_time-analyze_time)))

    # Stats tables processes
    proc_refs = Process(target=compute_domain_score,
                        args=(log_day, tuples_table))
    proc_ips = Process(target=compute_ip_score, args=(log_day, tuples_table))
    proc_ips.start()
    proc_refs.start()

    proc_refs.join()
    proc_ips.join()

    # Create index process
    proc_idx = Process(target=create_indexes, args=(tuples_table,))
    proc_idx.start()
    proc_idx.join()

    final_time = time()
    print("Total time {}".format(timedelta(seconds=final_time-start_time)))
    sys.exit(0)


def create_indexes(_table_name):
    """Create indexes on IP and referrer columns of the given tuples table."""
    _idx0_time = time()
    _conn = psycopg2.connect("dbname=%s user=%s port=%s"
                             % (POSTGRES_DB, POSTGRES_USER, POSTGRES_PORT))
    _cur = _conn.cursor()
    _cur.execute("CREATE INDEX ON {} (ip)".format(_table_name))
    _cur.execute("CREATE INDEX ON {} (referrer)".format(_table_name))
    _cur.execute("ANALYZE {}".format(_table_name))
    _conn.commit()
    _idx1_time = time()
    print("Created tuples indexes in {}".format(
            timedelta(seconds=_idx1_time-_idx0_time)))
    return


def compute_domain_score(_log_day, _tuples_table):
    """Compute the domain scores and add them to stats.referrer table."""
    _refs0_time = time()
    _conn = psycopg2.connect("dbname=%s user=%s port=%s"
                             % (POSTGRES_DB, POSTGRES_USER, POSTGRES_PORT))
    _cur = _conn.cursor()
    _cur.execute("SET ROLE nameles")
    _conn.commit()
    print("Creating referrer's stats table")
    _cur.execute("""CREATE TABLE stats.referrer_{0} AS
                    (SELECT referrer, sum(cnt) AS total_{0},
                     norm_entropy(cnt) AS score_{0}
                     FROM {1} GROUP BY referrer)
                     """.format(_log_day, _tuples_table))
    _conn.commit()
    _cur.execute("ANALYZE stats.referrer_{0}".format(_log_day))
    _conn.commit()
    _refs1_time = time()
    print("Created referrer's stats table in {}"
          .format(timedelta(seconds=_refs1_time-_refs0_time)))
    query = """
        BEGIN;
        LOCK TABLE stats.referrer IN EXCLUSIVE MODE;
        ALTER TABLE stats.referrer DROP CONSTRAINT IF EXISTS referrer_pkey;
        ALTER TABLE stats.referrer ADD COLUMN total_{0} int8 default NULL,
                                   ADD COLUMN score_{0} int4 default null;
        UPDATE stats.referrer m
        SET (total_{0}, score_{0}) = (d.total_{0}, d.score_{0})
        FROM stats.referrer_{0} d
        WHERE d.total_{0}>=50 and m.referrer=d.referrer;

        INSERT INTO stats.referrer (referrer, total_{0}, score_{0})
        SELECT d.referrer, d.total_{0}, d.score_{0} FROM stats.referrer_{0} d
        LEFT OUTER JOIN stats.referrer m ON (d.referrer = m.referrer)
        WHERE d.total_{0}>=50 and m.referrer IS NULL;
        COMMIT;
        """.format(_log_day)

    print("Merging referrer's stats table")
    _cur.execute(query)
    _conn.commit()
    _cur.execute("ALTER TABLE stats.referrer ADD PRIMARY KEY (referrer);")
    _conn.commit()
    _cur.execute("ANALYZE stats.referrer")
    _cur.execute("DROP TABLE stats.referrer_{0}".format(_log_day))
    _conn.commit()
    _refs2_time = time()
    print("Merged referrer's stats table in {}"
          .format(timedelta(seconds=_refs2_time-_refs1_time)))
    return


def compute_ip_score(_log_day, _tuples_table):
    """Compute the IP scores and add them to stats.referrer table."""
    _ips0_time = time()
    _conn = psycopg2.connect("dbname=%s user=%s port=%s"
                             % (POSTGRES_DB, POSTGRES_USER, POSTGRES_PORT))
    _cur = _conn.cursor()
    _cur.execute("SET ROLE nameles")
    _conn.commit()
    print("Creating IP's stats table")
    _cur.execute("""
                 CREATE TABLE stats.ip_{0} AS
                 (SELECT ip, sum(cnt) AS total_{0}, norm_entropy(cnt)
                 AS score_{0} FROM {1} GROUP BY ip)
                 """.format(_log_day, _tuples_table))
    _conn.commit()
    _cur.execute("ANALYZE stats.ip_{0}".format(_log_day))
    _conn.commit()
    _ips1_time = time()
    print("Created IP's stats table in {}"
          .format(timedelta(seconds=_ips1_time-_ips0_time)))
    query = """
        BEGIN;
        LOCK TABLE stats.ip IN EXCLUSIVE MODE;
        ALTER TABLE stats.ip DROP CONSTRAINT IF EXISTS ip_pkey;
        ALTER TABLE stats.ip ADD COLUMN total_{0} int8 default NULL,
                             ADD COLUMN score_{0} int4 default null;
        UPDATE stats.ip m
        SET (total_{0}, score_{0}) = (d.total_{0}, d.score_{0})
        FROM stats.ip_{0} d
        WHERE d.total_{0}>=50 and d.ip=m.ip;

        INSERT INTO stats.ip (ip, total_{0}, score_{0})
        SELECT d.ip, d.total_{0}, d.score_{0} FROM stats.ip_{0} d
        LEFT OUTER JOIN stats.ip m ON (m.ip = d.ip)
        WHERE d.total_{0}>=50 and m.ip IS NULL;
        COMMIT;
        """.format(_log_day)

    print("Merging IP's stats table")
    _cur.execute(query)
    _conn.commit()
    _cur.execute("ALTER TABLE stats.ip ADD PRIMARY KEY (ip);")
    _conn.commit()
    _cur.execute("ANALYZE stats.ip")
    _cur.execute("DROP TABLE stats.ip_{0}".format(_log_day))
    _conn.commit()
    _ips2_time = time()
    print("Merged IP's stats table in {}"
          .format(timedelta(seconds=_ips2_time-_ips1_time)))
    return


@context()
@socket(zmq.PUSH)
def notify_scores_updated(ctx, push, sockets, date=None):
    """Notify to the given zmq socket the date of new scores."""
    if not date:
        yesterday = datetime.date.today() - datetime.timedelta(days=1)
        date = yesterday.strftime("%y%m%d")
        for sckt in re.split(' +|, *|; *', sockets):
            push.conect(sckt)
            push.send_string(date)
            push.disconnect(sckt)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--date', help='date of the data')
    requiredArgs = parser.add_argument_group('Required arguments')
    requiredArgs.add_argument('-S', '--socket',
                              help=('notify new scores to the given sockets'
                                    '(format: "tcp://ip:port '
                                    '[tcp://ip:port]")'),
                              required=True)
    args = parser.parse_args()
    main(args)
