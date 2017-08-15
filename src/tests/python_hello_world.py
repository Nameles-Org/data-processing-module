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

import argparse

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--date', help='date of the data')
    requiredArgs = parser.add_argument_group('Required arguments')
    requiredArgs.add_argument('-S', '--socket', help='format: "tcp://ip:port',
                              required=True)
    args = parser.parse_args()
    print("Test script with arguments")
    print(args)
    print("END")
