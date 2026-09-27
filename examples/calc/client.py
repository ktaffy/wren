"""Calls every method of the example calc server with the generated client.

    make                          # generates build/gen/, builds the server
    ./build/calc_server 8080 &
    python client.py 8080
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent / "build" / "gen"))

from calc import Calc, Point  # noqa: E402
from wren import Client, WrenCallError  # noqa: E402


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    with Calc(Client("localhost", port)) as calc:
        print("add(3, 4) =", calc.add(3, 4))
        print("multiply(6, 7) =", calc.multiply(6, 7))
        print("distance_between((0, 0), (3, 4)) =",
              calc.distance_between(Point(0.0, 0.0), Point(3.0, 4.0)))
        print("sum_array([1, 2, 3, 100]) =", calc.sum_array([1, 2, 3, 100]))
        print("divmod(17, 5) =", calc.divmod(17, 5))
        try:
            calc.divmod(1, 0)
        except WrenCallError as e:
            print(f"divmod(1, 0) failed: code {e.code}: {e.message}")
        print("list_admins() =", calc.list_admins())
        calc.log_message("hello from Python")
        print("log_message sent (see the server's output)")


if __name__ == "__main__":
    main()