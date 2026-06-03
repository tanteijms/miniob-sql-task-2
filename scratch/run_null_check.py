import os
import shutil
import sys

sys.path.append('test/case')
import miniob_test


def main():
  base_dir = '/miniob/build'
  data_dir = '/tmp/miniob-null-harness'
  config = '/miniob/etc/observer.ini'
  port = 6801

  if os.path.exists(data_dir):
    shutil.rmtree(data_dir)

  miniob_test.GlobalConfig.debug = True

  sqls = [
      "create table null_table(id int not null, num int null, price float not null, birthday date null);",
      "create table null_table2(id int not null, num int null, price float not null, birthday date null);",
      "create index index_num on null_table(num);",
      "insert into null_table values (1, 18, 10.0, '2020-01-01');",
      "insert into null_table values (2, null, 20.0, '2010-01-11');",
      "insert into null_table values (3, 12, 30.0, null);",
      "insert into null_table values (4, 15, 30.0, '2021-01-31');",
      "insert into null_table2 values (1, 18, 30.0, '2021-01-31');",
      "insert into null_table2 values (2, null, 40.0, null);",
      "insert into null_table values (5, 15, null, '2021-01-31');",
      "insert into null_table values (null, 15, 30.0, '2021-01-31');",
      "select * from null_table;",
      "select * from null_table where 1 is null;",
      "select * from null_table where 1 is not null;",
      "select * from null_table where null is null;",
      "select * from null_table where null is not null;",
      "select * from null_table where num is null;",
      "select * from null_table where num is not null;",
      "select * from null_table where num = null;",
      "select * from null_table where null = num;",
      "select * from null_table where num <> null;",
      "select null_table.num,null_table2.num,null_table.birthday from null_table,null_table2 where null_table.num=null_table2.num;",
      "select count(*) from null_table;",
      "select count(price) from null_table;",
      "select count(birthday) from null_table;",
      "select avg(num) from null_table;",
      "create table null_table3(id int not null, num int null);",
      "insert into null_table3 values (1, null);",
      "insert into null_table3 values (2, null);",
      "select count(num) from null_table3;",
      "select min(num) from null_table3;",
      "select max(num) from null_table3;",
      "select avg(num) from null_table3;",
      "update null_table set num = null where id = 4;",
      "select * from null_table where num is null;",
      "update null_table set num = 99 where id = 2;",
      "select * from null_table where id = 2;",
      "update null_table set price = null where id = 1;",
      "select * from null_table where num = 99;",
  ]

  with miniob_test.MiniObServer(base_dir, data_dir, config, port, "", True) as server:
    server.init_server()
    if not server.start_server():
      print("FAILED TO START SERVER")
      return 2

    client = miniob_test.MiniObClient(port, "", time_limit=10)
    if not client.is_valid():
      print("FAILED TO CONNECT CLIENT")
      return 3

    try:
      for sql in sqls:
        ok, result = client.run_sql(sql)
        print("SQL:", sql)
        print("OK:", ok)
        print("RESULT_START")
        print("" if result is None else result, end="")
        print("RESULT_END")
        print("-----")
        if not ok:
          return 4
    finally:
      client.close()

  return 0


if __name__ == "__main__":
  raise SystemExit(main())
