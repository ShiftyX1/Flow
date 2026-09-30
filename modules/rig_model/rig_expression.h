#pragma once

#include "core/string/string_name.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"

// Variable storage shared by every expression evaluated for one rig.
//
// Keys are normalised, lowercase names: "query.ground_speed", "variable.swing", "temp.x".
// Missing keys read as 0.
class RigExpressionContext {
public:
	HashMap<StringName, float> values;

	float get(const StringName &p_name) const {
		const float *v = values.getptr(p_name);
		return v ? *v : 0.0f;
	}
	void set(const StringName &p_name, float p_value) { values[p_name] = p_value; }
};

// Molang-lite expression.
//
// Supported syntax:
//   - numbers, parentheses, unary - and !
//   - + - * / %, comparisons (< <= > >= == !=), && ||, ternary "a ? b : c" (and "a ? b")
//   - identifiers: query.* / q.*, variable.* / v.*, temp.* / t.* (case-insensitive)
//   - assignments "variable.x = expr;" and "return expr;" in a ';'-separated script
//   - math.* functions, angles in DEGREES (sin, cos, asin, acos, atan, atan2), plus abs, sqrt,
//     floor, ceil, round, trunc, clamp, min, max, lerp, pow, mod, exp, ln, random, min_angle
//     and the constant math.pi
//
// The expression is parsed once; evaluation does not allocate.
class RigExpression {
public:
	enum Op : uint8_t {
		OP_CONST,
		OP_VAR,
		OP_NEG,
		OP_NOT,
		OP_ADD,
		OP_SUB,
		OP_MUL,
		OP_DIV,
		OP_MOD,
		OP_LT,
		OP_LE,
		OP_GT,
		OP_GE,
		OP_EQ,
		OP_NE,
		OP_AND,
		OP_OR,
		OP_TERNARY,
		OP_CALL,
		OP_ASSIGN,
		OP_RETURN,
	};

	enum Fn : uint8_t {
		FN_SIN,
		FN_COS,
		FN_ASIN,
		FN_ACOS,
		FN_ATAN,
		FN_ATAN2,
		FN_ABS,
		FN_SQRT,
		FN_FLOOR,
		FN_CEIL,
		FN_ROUND,
		FN_TRUNC,
		FN_CLAMP,
		FN_MIN,
		FN_MAX,
		FN_LERP,
		FN_POW,
		FN_MOD,
		FN_EXP,
		FN_LN,
		FN_RANDOM,
		FN_MIN_ANGLE,
	};

private:
	struct Node {
		Op op = OP_CONST;
		Fn fn = FN_SIN;
		float value = 0.0f;
		StringName name;
		int a = -1;
		int b = -1;
		int c = -1;
		Vector<int> args;
	};

	Vector<Node> nodes;
	Vector<int> roots;
	bool constant = true;
	float constant_value = 0.0f;
	String source;
	String error;

	float _eval(int p_node, RigExpressionContext &p_ctx) const;

	friend class RigExpressionParser;

public:
	static StringName normalize_name(const String &p_name);

	// Parses p_source. On failure the expression evaluates to 0 and get_error() is set.
	Error parse(const String &p_source);
	void set_constant(float p_value);

	bool is_valid() const { return error.is_empty(); }
	bool is_constant() const { return constant; }
	bool is_empty() const { return roots.is_empty(); }
	const String &get_error() const { return error; }
	const String &get_source() const { return source; }

	float evaluate(RigExpressionContext &p_ctx) const {
		if (constant) {
			return constant_value;
		}
		float result = 0.0f;
		for (int i = 0; i < roots.size(); i++) {
			const Node &n = nodes[roots[i]];
			if (n.op == OP_RETURN) {
				return _eval(n.a, p_ctx);
			}
			result = _eval(roots[i], p_ctx);
		}
		return result;
	}
};
