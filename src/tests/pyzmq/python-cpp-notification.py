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

"""Script for testing zmq messaging from python to C++."""

import zmq
import datetime
from zmq.decorators import context, socket


@context()
@socket(zmq.PUSH)
def notify_scores_updated(sockets, date, ctx, push):
    """Notify to the given zmq socket the date of new scores."""
    if not date:
        yesterday = datetime.date.today() - datetime.timedelta(days=1)
        date = yesterday.strftime("%y%m%d")
    print(ctx, push, sockets, date)
    for sckt in sockets.split():
        push.connect(sckt)
        push.send_string(date)
        print("Sent date: ", date)
        push.disconnect(sckt)


if __name__ == "__main__":
    context = zmq.Context()
    socket = context.socket(zmq.PUSH)
    notify_scores_updated("tcp://127.0.0.1:1138 tcp://127.0.0.1:1138",
                          "171123")
    print("msg 1 sent")
    notify_scores_updated("tcp://127.0.0.1:1138", "174452")
    print("msg 2 sent")
    notify_scores_updated("tcp://127.0.0.1:1138", "kill")
    print("msg 3 sent")
    print("END OF TEST")
