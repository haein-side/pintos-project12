#define F (1<<14)             // (0 00000000000000001 00000000000000)
#define INT_MAX ((1<<31) - 1) // (0 11111111111111111 11111111111111)
#define INT_MIN (-(1<<31))    // (1 00000000000000000 00000000000000)

// 매개변수 x,y 도 fixed-point형식의 실수
// n은 정수를 나타냄

// Convert n to fixed point
int int_to_fp (int n) {
    return n * F;
}
// Convert x to integer (rounding toward zero)
int fp_to_int (int x) {
    return x / F;
}

// convert x to integer
// F/2는 fixed point 0.5를 뜻함. 0.5를 더한 후 버림하면 반올림이 됨.
int fp_to_int_round (int x) {
    if (x >= 0) return (x + F / 2) / F;
    else return (x - f / 2) / f;
}
// Add x and y
int add_fp(int x, int y) {
    return x+y;
}
// Subtract y from x
int sub_fp (int x, int y) {
    return x-y;
}
// Add x and n
int add_mixed (int x, int n) {
    return x + n * F;
}
// Subtract n from x
int sub_mixed (int x, int n) {
    return x - n * F;
}
// Multiply x by y
/* 두 개의 fp 를 곱하면 32bit * 32bit 로 32bit 를 초과하는 값이 발생하기 때문에 
x 를 64bit 로 일시적으로 바꿔준 후 곱하기 연산을 하여 오버플로우를 방지하고 계산 결과로 
나온 64bit 값을 F 로 나눠서 다시 32bit 값으로 만들어 준다. (div_fp 도 마찬가지)
*/
int mult_fp (int x, int y) {
    return ((int64_t)x) * y / F;
}
// Multiply x by n
int mult_mixed (int x, int n) {
    return x * n;
}
// Divide x by y
int div_fp (int x, int y) {
    return ((int64_t) x) * F / y;
}
// Divide x by n
int div_mixed (int x, int y) {
    return x / y;
}