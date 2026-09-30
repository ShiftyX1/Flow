/**************************************************************************/
/*  test_rig_model.h                                                      */
/**************************************************************************/

#pragma once

#include "../rig_animation_data.h"
#include "../rig_expression.h"
#include "../rig_model_data.h"

#include "tests/test_macros.h"

namespace TestRigModel {

static float _eval(const String &p_source, RigExpressionContext &p_ctx) {
	RigExpression expr;
	CHECK(expr.parse(p_source) == OK);
	return expr.evaluate(p_ctx);
}

TEST_CASE("[RigModel] Expression arithmetic and precedence") {
	RigExpressionContext ctx;
	CHECK(_eval("1 + 2 * 3", ctx) == doctest::Approx(7.0f));
	CHECK(_eval("(1 + 2) * 3", ctx) == doctest::Approx(9.0f));
	CHECK(_eval("-2 + 5", ctx) == doctest::Approx(3.0f));
	CHECK(_eval("1 < 2 && 3 > 2", ctx) == doctest::Approx(1.0f));
	CHECK(_eval("0 || !0", ctx) == doctest::Approx(1.0f));
	CHECK(_eval("1 ? 10 : 20", ctx) == doctest::Approx(10.0f));
	CHECK(_eval("0 ? 10 : 20", ctx) == doctest::Approx(20.0f));
}

TEST_CASE("[RigModel] Expression variables and math functions use degrees") {
	RigExpressionContext ctx;
	ctx.set(RigExpression::normalize_name("query.speed"), 2.0f);
	CHECK(_eval("q.speed * 3", ctx) == doctest::Approx(6.0f));
	CHECK(_eval("math.sin(90)", ctx) == doctest::Approx(1.0f));
	CHECK(_eval("math.cos(0)", ctx) == doctest::Approx(1.0f));
	CHECK(_eval("math.clamp(5, 0, 1)", ctx) == doctest::Approx(1.0f));
	CHECK(_eval("math.min(4, 2)", ctx) == doctest::Approx(2.0f));
	CHECK(_eval("math.abs(-3)", ctx) == doctest::Approx(3.0f));
}

TEST_CASE("[RigModel] Expression scripts assign variables and return") {
	RigExpressionContext ctx;
	CHECK(_eval("variable.a = 4; variable.b = variable.a * 2; return variable.b + 1;", ctx) == doctest::Approx(9.0f));
	CHECK(ctx.get(RigExpression::normalize_name("v.a")) == doctest::Approx(4.0f));
}

TEST_CASE("[RigModel] Invalid expression reports an error and evaluates to zero") {
	RigExpression expr;
	CHECK(expr.parse("1 + * 2") != OK);
	CHECK_FALSE(expr.is_valid());
	RigExpressionContext ctx;
	CHECK(expr.evaluate(ctx) == doctest::Approx(0.0f));
}

TEST_CASE("[RigModel] Geometry parsing builds bones and cubes") {
	const String json = R"({
		"format_version": "1.12.0",
		"minecraft:geometry": [{
			"description": {"identifier": "geometry.test", "texture_width": 16, "texture_height": 16},
			"bones": [
				{"name": "root", "pivot": [0, 0, 0]},
				{"name": "arm", "parent": "root", "pivot": [0, 8, 0],
					"cubes": [{"origin": [-1, 0, -1], "size": [2, 8, 2], "uv": [0, 0]}]}
			]
		}]
	})";

	Ref<RigModelData> model;
	model.instantiate();
	REQUIRE(model->parse_json(json) == OK);
	CHECK(model->get_bone_count() == 2);
	const PackedStringArray names = model->get_bone_names();
	CHECK(names.has("root"));
	CHECK(names.has("arm"));
}

TEST_CASE("[RigModel] Animation clips are found by exact and suffix name") {
	const String json = R"({
		"format_version": "1.8.0",
		"animations": {
			"animation.test.wave": {
				"loop": true,
				"animation_length": 2.0,
				"bones": {"arm": {"rotation": {"0.0": [0, 0, 0], "2.0": [0, 0, 90]}}}
			}
		}
	})";

	Ref<RigAnimationData> anim;
	anim.instantiate();
	REQUIRE(anim->parse_json(json) == OK);
	CHECK(anim->has_animation("animation.test.wave"));
	CHECK(anim->has_animation("wave"));
	CHECK_FALSE(anim->has_animation("missing"));
	CHECK(anim->get_animation_length("wave") == doctest::Approx(2.0f));
}

TEST_CASE("[RigModel] Clip pre_animation scripts are parsed and share variables") {
	const String json = R"({
		"animations": {
			"animation.test.vars": {
				"loop": true,
				"animation_length": 1.0,
				"pre_animation": ["variable.a = 3", "variable.b = variable.a * 2"],
				"bones": {}
			}
		}
	})";

	Ref<RigAnimationData> anim;
	anim.instantiate();
	REQUIRE(anim->parse_json(json) == OK);
	const RigAnimationData::Clip *clip = anim->find_clip("vars");
	REQUIRE(clip != nullptr);
	REQUIRE(clip->pre_animation.size() == 2);

	RigExpressionContext ctx;
	for (int i = 0; i < clip->pre_animation.size(); i++) {
		clip->pre_animation[i].evaluate(ctx);
	}
	CHECK(ctx.get(RigExpression::normalize_name("variable.b")) == doctest::Approx(6.0f));
}

} // namespace TestRigModel
