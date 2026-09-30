#include "rig_expression.h"

#include "core/math/math_funcs.h"
#include "core/variant/variant.h"

namespace {

struct FnInfo {
	const char *name;
	RigExpression::Fn fn;
	int min_args;
	int max_args;
};

const FnInfo FN_TABLE[] = {
	{ "sin", RigExpression::FN_SIN, 1, 1 },
	{ "cos", RigExpression::FN_COS, 1, 1 },
	{ "asin", RigExpression::FN_ASIN, 1, 1 },
	{ "acos", RigExpression::FN_ACOS, 1, 1 },
	{ "atan", RigExpression::FN_ATAN, 1, 1 },
	{ "atan2", RigExpression::FN_ATAN2, 2, 2 },
	{ "abs", RigExpression::FN_ABS, 1, 1 },
	{ "sqrt", RigExpression::FN_SQRT, 1, 1 },
	{ "floor", RigExpression::FN_FLOOR, 1, 1 },
	{ "ceil", RigExpression::FN_CEIL, 1, 1 },
	{ "round", RigExpression::FN_ROUND, 1, 1 },
	{ "trunc", RigExpression::FN_TRUNC, 1, 1 },
	{ "clamp", RigExpression::FN_CLAMP, 3, 3 },
	{ "min", RigExpression::FN_MIN, 2, 2 },
	{ "max", RigExpression::FN_MAX, 2, 2 },
	{ "lerp", RigExpression::FN_LERP, 3, 3 },
	{ "pow", RigExpression::FN_POW, 2, 2 },
	{ "mod", RigExpression::FN_MOD, 2, 2 },
	{ "exp", RigExpression::FN_EXP, 1, 1 },
	{ "ln", RigExpression::FN_LN, 1, 1 },
	{ "random", RigExpression::FN_RANDOM, 2, 2 },
	{ "min_angle", RigExpression::FN_MIN_ANGLE, 1, 1 },
};

} // namespace

StringName RigExpression::normalize_name(const String &p_name) {
	String n = p_name.strip_edges().to_lower();
	if (n.begins_with("q.")) {
		n = "query." + n.substr(2);
	} else if (n.begins_with("v.")) {
		n = "variable." + n.substr(2);
	} else if (n.begins_with("t.")) {
		n = "temp." + n.substr(2);
	}
	return StringName(n);
}

class RigExpressionParser {
	struct Token {
		enum Type {
			END,
			NUM,
			IDENT,
			OP,
		};
		Type type = END;
		float num = 0.0f;
		String text;
	};

	RigExpression &out;
	Vector<Token> tokens;
	int pos = 0;
	String error;

	const Token &peek(int p_ahead = 0) const {
		static const Token end_token;
		int i = pos + p_ahead;
		return i < tokens.size() ? tokens[i] : end_token;
	}

	bool is_op(const char *p_text, int p_ahead = 0) const {
		const Token &t = peek(p_ahead);
		return t.type == Token::OP && t.text == p_text;
	}

	bool accept_op(const char *p_text) {
		if (is_op(p_text)) {
			pos++;
			return true;
		}
		return false;
	}

	int add(const RigExpression::Node &p_node) {
		out.nodes.push_back(p_node);
		return out.nodes.size() - 1;
	}

	int fail(const String &p_msg) {
		if (error.is_empty()) {
			error = p_msg;
		}
		return -1;
	}

	bool tokenize(const String &p_src) {
		const int n = p_src.length();
		int i = 0;
		while (i < n) {
			const char32_t c = p_src[i];
			if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
				i++;
				continue;
			}
			if (is_digit(c) || (c == '.' && i + 1 < n && is_digit(p_src[i + 1]))) {
				int start = i;
				while (i < n && (is_digit(p_src[i]) || p_src[i] == '.')) {
					i++;
				}
				Token t;
				t.type = Token::NUM;
				t.text = p_src.substr(start, i - start);
				t.num = t.text.to_float();
				tokens.push_back(t);
				continue;
			}
			if (is_ascii_alphabet_char(c) || c == '_') {
				int start = i;
				while (i < n && (is_ascii_alphanumeric_char(p_src[i]) || p_src[i] == '_' || p_src[i] == '.')) {
					i++;
				}
				Token t;
				t.type = Token::IDENT;
				t.text = p_src.substr(start, i - start);
				tokens.push_back(t);
				continue;
			}
			static const char *two_char_ops[] = { "<=", ">=", "==", "!=", "&&", "||" };
			bool matched = false;
			if (i + 1 < n) {
				for (const char *op : two_char_ops) {
					if (p_src[i] == op[0] && p_src[i + 1] == op[1]) {
						Token t;
						t.type = Token::OP;
						t.text = op;
						tokens.push_back(t);
						i += 2;
						matched = true;
						break;
					}
				}
			}
			if (matched) {
				continue;
			}
			static const char *single_ops = "+-*/%<>!?:(),;=";
			bool single = false;
			for (const char *s = single_ops; *s; s++) {
				if (c == (char32_t)*s) {
					single = true;
					break;
				}
			}
			if (!single) {
				fail(vformat("unexpected character '%s'", String::chr(c)));
				return false;
			}
			Token t;
			t.type = Token::OP;
			t.text = String::chr(c);
			tokens.push_back(t);
			i++;
		}
		return true;
	}

	int parse_ternary() {
		int cond = parse_or();
		if (cond < 0) {
			return -1;
		}
		if (accept_op("?")) {
			int a = parse_ternary();
			if (a < 0) {
				return -1;
			}
			int b = -1;
			if (accept_op(":")) {
				b = parse_ternary();
				if (b < 0) {
					return -1;
				}
			}
			RigExpression::Node n;
			n.op = RigExpression::OP_TERNARY;
			n.a = cond;
			n.b = a;
			n.c = b;
			return add(n);
		}
		return cond;
	}

	int binary(RigExpression::Op p_op, int p_a, int p_b) {
		RigExpression::Node n;
		n.op = p_op;
		n.a = p_a;
		n.b = p_b;
		return add(n);
	}

	int parse_or() {
		int left = parse_and();
		while (left >= 0 && accept_op("||")) {
			int right = parse_and();
			if (right < 0) {
				return -1;
			}
			left = binary(RigExpression::OP_OR, left, right);
		}
		return left;
	}

	int parse_and() {
		int left = parse_equality();
		while (left >= 0 && accept_op("&&")) {
			int right = parse_equality();
			if (right < 0) {
				return -1;
			}
			left = binary(RigExpression::OP_AND, left, right);
		}
		return left;
	}

	int parse_equality() {
		int left = parse_relational();
		while (left >= 0) {
			RigExpression::Op op;
			if (accept_op("==")) {
				op = RigExpression::OP_EQ;
			} else if (accept_op("!=")) {
				op = RigExpression::OP_NE;
			} else {
				break;
			}
			int right = parse_relational();
			if (right < 0) {
				return -1;
			}
			left = binary(op, left, right);
		}
		return left;
	}

	int parse_relational() {
		int left = parse_additive();
		while (left >= 0) {
			RigExpression::Op op;
			if (accept_op("<=")) {
				op = RigExpression::OP_LE;
			} else if (accept_op(">=")) {
				op = RigExpression::OP_GE;
			} else if (accept_op("<")) {
				op = RigExpression::OP_LT;
			} else if (accept_op(">")) {
				op = RigExpression::OP_GT;
			} else {
				break;
			}
			int right = parse_additive();
			if (right < 0) {
				return -1;
			}
			left = binary(op, left, right);
		}
		return left;
	}

	int parse_additive() {
		int left = parse_multiplicative();
		while (left >= 0) {
			RigExpression::Op op;
			if (accept_op("+")) {
				op = RigExpression::OP_ADD;
			} else if (accept_op("-")) {
				op = RigExpression::OP_SUB;
			} else {
				break;
			}
			int right = parse_multiplicative();
			if (right < 0) {
				return -1;
			}
			left = binary(op, left, right);
		}
		return left;
	}

	int parse_multiplicative() {
		int left = parse_unary();
		while (left >= 0) {
			RigExpression::Op op;
			if (accept_op("*")) {
				op = RigExpression::OP_MUL;
			} else if (accept_op("/")) {
				op = RigExpression::OP_DIV;
			} else if (accept_op("%")) {
				op = RigExpression::OP_MOD;
			} else {
				break;
			}
			int right = parse_unary();
			if (right < 0) {
				return -1;
			}
			left = binary(op, left, right);
		}
		return left;
	}

	int parse_unary() {
		if (accept_op("-")) {
			int a = parse_unary();
			if (a < 0) {
				return -1;
			}
			RigExpression::Node n;
			n.op = RigExpression::OP_NEG;
			n.a = a;
			return add(n);
		}
		if (accept_op("+")) {
			return parse_unary();
		}
		if (accept_op("!")) {
			int a = parse_unary();
			if (a < 0) {
				return -1;
			}
			RigExpression::Node n;
			n.op = RigExpression::OP_NOT;
			n.a = a;
			return add(n);
		}
		return parse_primary();
	}

	int make_const(float p_value) {
		RigExpression::Node n;
		n.op = RigExpression::OP_CONST;
		n.value = p_value;
		return add(n);
	}

	int parse_primary() {
		const Token t = peek();
		if (t.type == Token::NUM) {
			pos++;
			return make_const(t.num);
		}
		if (t.type == Token::OP && t.text == "(") {
			pos++;
			int inner = parse_ternary();
			if (inner < 0) {
				return -1;
			}
			if (!accept_op(")")) {
				return fail("expected ')'");
			}
			return inner;
		}
		if (t.type == Token::IDENT) {
			pos++;
			const String lower = t.text.to_lower();
			if (is_op("(")) {
				return parse_call(lower);
			}
			if (lower == "true") {
				return make_const(1.0f);
			}
			if (lower == "false") {
				return make_const(0.0f);
			}
			if (lower == "math.pi") {
				return make_const((float)Math::PI);
			}
			RigExpression::Node n;
			n.op = RigExpression::OP_VAR;
			n.name = RigExpression::normalize_name(lower);
			return add(n);
		}
		return fail("unexpected end of expression or token");
	}

	int parse_call(const String &p_name) {
		String fname = p_name;
		if (fname.begins_with("math.")) {
			fname = fname.substr(5);
		}
		const FnInfo *info = nullptr;
		for (const FnInfo &fi : FN_TABLE) {
			if (fname == fi.name) {
				info = &fi;
				break;
			}
		}
		if (!info) {
			return fail(vformat("unknown function '%s'", p_name));
		}
		pos++; // '('
		Vector<int> args;
		if (!is_op(")")) {
			while (true) {
				int arg = parse_ternary();
				if (arg < 0) {
					return -1;
				}
				args.push_back(arg);
				if (accept_op(",")) {
					continue;
				}
				break;
			}
		}
		if (!accept_op(")")) {
			return fail("expected ')' after function arguments");
		}
		if (args.size() < info->min_args || args.size() > info->max_args) {
			return fail(vformat("wrong argument count for '%s'", p_name));
		}
		RigExpression::Node n;
		n.op = RigExpression::OP_CALL;
		n.fn = info->fn;
		n.args = args;
		return add(n);
	}

	int parse_statement() {
		const Token &t = peek();
		if (t.type == Token::IDENT && t.text.to_lower() == "return") {
			pos++;
			int value = parse_ternary();
			if (value < 0) {
				return -1;
			}
			RigExpression::Node n;
			n.op = RigExpression::OP_RETURN;
			n.a = value;
			return add(n);
		}
		if (t.type == Token::IDENT && is_op("=", 1)) {
			const StringName target = RigExpression::normalize_name(t.text);
			pos += 2;
			int value = parse_ternary();
			if (value < 0) {
				return -1;
			}
			RigExpression::Node n;
			n.op = RigExpression::OP_ASSIGN;
			n.name = target;
			n.a = value;
			return add(n);
		}
		return parse_ternary();
	}

public:
	explicit RigExpressionParser(RigExpression &p_out) :
			out(p_out) {}

	String run(const String &p_src) {
		if (!tokenize(p_src)) {
			return error;
		}
		while (peek().type != Token::END) {
			if (accept_op(";")) {
				continue;
			}
			int stmt = parse_statement();
			if (stmt < 0) {
				return error;
			}
			out.roots.push_back(stmt);
			if (peek().type != Token::END && !is_op(";")) {
				fail("expected ';' or end of expression");
				return error;
			}
		}
		return error;
	}
};

Error RigExpression::parse(const String &p_source) {
	nodes.clear();
	roots.clear();
	source = p_source;
	error = String();
	constant = true;
	constant_value = 0.0f;

	const String trimmed = p_source.strip_edges();
	if (trimmed.is_empty()) {
		return OK;
	}
	if (trimmed.is_valid_float()) {
		constant_value = trimmed.to_float();
		return OK;
	}

	RigExpressionParser parser(*this);
	const String err = parser.run(trimmed);
	if (!err.is_empty()) {
		nodes.clear();
		roots.clear();
		constant = true;
		constant_value = 0.0f;
		error = err;
		return ERR_PARSE_ERROR;
	}

	// Fold trivial single-constant scripts.
	if (roots.size() == 1 && nodes[roots[0]].op == OP_CONST) {
		constant_value = nodes[roots[0]].value;
		nodes.clear();
		roots.clear();
		return OK;
	}
	constant = false;
	return OK;
}

void RigExpression::set_constant(float p_value) {
	nodes.clear();
	roots.clear();
	source = String::num(p_value);
	error = String();
	constant = true;
	constant_value = p_value;
}

float RigExpression::_eval(int p_node, RigExpressionContext &p_ctx) const {
	const Node &n = nodes[p_node];
	switch (n.op) {
		case OP_CONST:
			return n.value;
		case OP_VAR:
			return p_ctx.get(n.name);
		case OP_NEG:
			return -_eval(n.a, p_ctx);
		case OP_NOT:
			return _eval(n.a, p_ctx) == 0.0f ? 1.0f : 0.0f;
		case OP_ADD:
			return _eval(n.a, p_ctx) + _eval(n.b, p_ctx);
		case OP_SUB:
			return _eval(n.a, p_ctx) - _eval(n.b, p_ctx);
		case OP_MUL:
			return _eval(n.a, p_ctx) * _eval(n.b, p_ctx);
		case OP_DIV: {
			const float d = _eval(n.b, p_ctx);
			return d == 0.0f ? 0.0f : _eval(n.a, p_ctx) / d;
		}
		case OP_MOD: {
			const float d = _eval(n.b, p_ctx);
			return d == 0.0f ? 0.0f : Math::fmod(_eval(n.a, p_ctx), d);
		}
		case OP_LT:
			return _eval(n.a, p_ctx) < _eval(n.b, p_ctx) ? 1.0f : 0.0f;
		case OP_LE:
			return _eval(n.a, p_ctx) <= _eval(n.b, p_ctx) ? 1.0f : 0.0f;
		case OP_GT:
			return _eval(n.a, p_ctx) > _eval(n.b, p_ctx) ? 1.0f : 0.0f;
		case OP_GE:
			return _eval(n.a, p_ctx) >= _eval(n.b, p_ctx) ? 1.0f : 0.0f;
		case OP_EQ:
			return Math::is_equal_approx(_eval(n.a, p_ctx), _eval(n.b, p_ctx)) ? 1.0f : 0.0f;
		case OP_NE:
			return Math::is_equal_approx(_eval(n.a, p_ctx), _eval(n.b, p_ctx)) ? 0.0f : 1.0f;
		case OP_AND:
			return (_eval(n.a, p_ctx) != 0.0f && _eval(n.b, p_ctx) != 0.0f) ? 1.0f : 0.0f;
		case OP_OR:
			return (_eval(n.a, p_ctx) != 0.0f || _eval(n.b, p_ctx) != 0.0f) ? 1.0f : 0.0f;
		case OP_TERNARY:
			if (_eval(n.a, p_ctx) != 0.0f) {
				return _eval(n.b, p_ctx);
			}
			return n.c >= 0 ? _eval(n.c, p_ctx) : 0.0f;
		case OP_ASSIGN: {
			const float v = _eval(n.a, p_ctx);
			p_ctx.set(n.name, v);
			return v;
		}
		case OP_RETURN:
			return _eval(n.a, p_ctx);
		case OP_CALL: {
			const int argc = n.args.size();
			const float x = argc > 0 ? _eval(n.args[0], p_ctx) : 0.0f;
			const float y = argc > 1 ? _eval(n.args[1], p_ctx) : 0.0f;
			switch (n.fn) {
				case FN_SIN:
					return Math::sin(Math::deg_to_rad(x));
				case FN_COS:
					return Math::cos(Math::deg_to_rad(x));
				case FN_ASIN:
					return Math::rad_to_deg(Math::asin(CLAMP(x, -1.0f, 1.0f)));
				case FN_ACOS:
					return Math::rad_to_deg(Math::acos(CLAMP(x, -1.0f, 1.0f)));
				case FN_ATAN:
					return Math::rad_to_deg(Math::atan(x));
				case FN_ATAN2:
					return Math::rad_to_deg(Math::atan2(x, y));
				case FN_ABS:
					return Math::abs(x);
				case FN_SQRT:
					return x > 0.0f ? Math::sqrt(x) : 0.0f;
				case FN_FLOOR:
					return Math::floor(x);
				case FN_CEIL:
					return Math::ceil(x);
				case FN_ROUND:
					return Math::round(x);
				case FN_TRUNC:
					return x < 0.0f ? Math::ceil(x) : Math::floor(x);
				case FN_CLAMP: {
					const float hi = _eval(n.args[2], p_ctx);
					return CLAMP(x, y, MAX(y, hi));
				}
				case FN_MIN:
					return MIN(x, y);
				case FN_MAX:
					return MAX(x, y);
				case FN_LERP: {
					const float t = _eval(n.args[2], p_ctx);
					return x + (y - x) * t;
				}
				case FN_POW:
					return Math::pow(x, y);
				case FN_MOD:
					return y == 0.0f ? 0.0f : Math::fmod(x, y);
				case FN_EXP:
					return Math::exp(x);
				case FN_LN:
					return x > 0.0f ? Math::log(x) : 0.0f;
				case FN_RANDOM:
					return x + (y - x) * Math::randf();
				case FN_MIN_ANGLE:
					return Math::fposmod(x + 180.0f, 360.0f) - 180.0f;
			}
			return 0.0f;
		}
	}
	return 0.0f;
}
