#pragma once
#include "cserialization/node.hpp"

/*
------------------------
NOTES
-----------------------

struct_csav.qwordE0 + 648   is ver1

--------------------------------------
cnt = read(4)

vec<8> a(cnt)
vec<16> b(cnt)

for i in range(cnt)
  c = read(8)
  d = read(4)
  if d:
    break  <- thx IDA for this aweful control flow.. (--') goes to the "while 1"

label_loop <-  

---------
if !d:



---------

while 1
  read_string_by_hashid_if_v193p()  <- question is, can i get a json of names so that i compute the hashes myself ?
  e = read(4)
  if VER1 >= 97:
    f = read(1)
  if VER1 >= 190:
    g = read(2)

*/

struct inventory
{
  void deserialize(const std::shared_ptr<const node_t>& node)
  {
    auto& data = node->data();

  }
};

