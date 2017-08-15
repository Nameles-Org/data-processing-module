/*
 * Copyright 2017 Antonio Pastor anpastor{at}it.uc3m.es
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <Python.h> // Always the first include
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <cwchar>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iostream>

int main(int argc, char *argv[]) {

  std::string strargv0(argv[0]);
  std::wstring argv0(strargv0.begin(),strargv0.end());
  std::wcout << "\"" << argv0 << "\" conversion OK. Length: " << argv0.length() << std::endl;
  wchar_t wcstr[argv0.length()+1];
  std::wcscpy (wcstr, argv0.c_str());
  std::wcout << wcstr << std::endl;

  wchar_t working_date[7];
  auto t = std::time(nullptr);
  auto tm = *std::localtime(&t);  // Always get the struct from localtime (copy the value). The pointer just gives a local buffer.
  auto ret = wcsftime(working_date, 7, L"%C%m%d\0", &tm);

  std::wcout << "wchar conversion of date OK. Ret val: " << ret << " *" << working_date << "*" << std::endl;
  wchar_t wargv0[] = L"python_hello_world.py";
  wchar_t wargv1[] = L"--date";
  wchar_t wargv3[] = L"--socket";
  wchar_t wargv4[] = L"tcp://127.0.0.1:1138";
  wchar_t *wargv[] = {&wargv0[0], &wargv1[0], &working_date[0], &wargv3[0], &wargv4[0]};

  Py_SetProgramName(wcstr);  /* optional but recommended */

  Py_Initialize();
  PySys_SetArgv(5, (wchar_t**) &wargv[0]);
  FILE *file = fopen("python_hello_world.py","r");
  PyRun_SimpleFile(file, "python_hello_world.py");
  fclose(file);
  Py_Finalize();
}
