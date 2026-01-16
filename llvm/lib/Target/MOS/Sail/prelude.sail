// MOS 65xx SAIL Specification - Prelude
// Includes, operators, and basic type overloads

default Order dec

$include <prelude.sail>
$include <smt.sail>
$include <option.sail>
$include <arith.sail>
$include <vector_dec.sail>
$include <string.sail>

// =============================================================================
// Bitwise Operators
// =============================================================================

overload operator & = {and_vec}
overload operator | = {or_vec}
overload operator ^ = {xor_vec}

// =============================================================================
// Arithmetic Operators
// =============================================================================

val sub_vec = pure {c: "sub_bits", _: "sub_vec"} : forall 'n. (bits('n), bits('n)) -> bits('n)
val sub_vec_int = pure {c: "sub_bits_int", _: "sub_vec_int"} : forall 'n. (bits('n), int) -> bits('n)
val add_vec = pure {c: "add_bits", _: "add_vec"} : forall 'n. (bits('n), bits('n)) -> bits('n)
val add_vec_int = pure {c: "add_bits_int", _: "add_vec_int"} : forall 'n. (bits('n), int) -> bits('n)

overload operator - = {sub_vec, sub_vec_int}
overload operator + = {add_vec, add_vec_int}

// =============================================================================
// Shift Operators
// =============================================================================

infix 7 >>
infix 7 <<

val "shift_bits_right" : forall 'n 'm. (bits('n), bits('m)) -> bits('n)
val "shift_bits_left"  : forall 'n 'm. (bits('n), bits('m)) -> bits('n)

val "shiftl" : forall 'm 'n, 'n >= 0. (bits('m), int('n)) -> bits('m)
val "shiftr" : forall 'm 'n, 'n >= 0. (bits('m), int('n)) -> bits('m)

overload operator >> = {shift_bits_right, shiftr}
overload operator << = {shift_bits_left, shiftl}

// =============================================================================
// Unsigned Comparison Operators
// =============================================================================

infix 4 >=_u
infix 4 <=_u
infix 4 >_u
infix 4 <_u

val operator >=_u : forall 'n. (bits('n), bits('n)) -> bool
val operator <=_u : forall 'n. (bits('n), bits('n)) -> bool
val operator >_u  : forall 'n. (bits('n), bits('n)) -> bool
val operator <_u  : forall 'n. (bits('n), bits('n)) -> bool

function operator >=_u (x, y) = unsigned(x) >= unsigned(y)
function operator <=_u (x, y) = unsigned(x) <= unsigned(y)
function operator >_u  (x, y) = unsigned(x) > unsigned(y)
function operator <_u  (x, y) = unsigned(x) < unsigned(y)

// =============================================================================
// Sign/Zero Extension
// =============================================================================

val sign_extend : forall 'n 'm, 'm >= 'n. (implicit('m), bits('n)) -> bits('m)
val zero_extend : forall 'n 'm, 'm >= 'n. (implicit('m), bits('n)) -> bits('m)

function sign_extend(m, x) = sail_sign_extend(x, m)
function zero_extend(m, x) = sail_zero_extend(x, m)
