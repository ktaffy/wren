// calc.wren

struct Point {
    f64 x;
    f64 y;
}

struct Row {
    u32 id;
    string name;
    string role;
}

service Calc {
    add(u32 a, u32 b) -> u32;
    multiply(u32 a, u32 b) -> u32;

    distance_between(Point p1, Point p2) -> f64;

    sum_array(u32[] values) -> u64;

    divmod(u32 a, u32 b) -> (u32 quotient, u32 remainder);

    list_admins() -> Row[];

    log_message(string msg);       // no return value
}