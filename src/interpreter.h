
#ifndef LOX_INTERPRETER
#define LOX_INTERPRETER
#include <string>
#include <variant>

#include "expression.h"
#include "token.h"
#include "utils/box.h"
#include "utils/error.h"
namespace {

using Object = std::variant<std::monostate, bool, double, std::string>;

template <typename... Objects>
auto check_number_operand(Token const& token, Objects... operands) -> void {
  if (std::all_of(std::begin(operands...), std::end(operands...),
                  [](Object const& obj) {
                    return !std::holds_alternative<double>(obj);
                  })) {
    throw RuntimeError{token, sizeof...(operands) > 1
                                  ? "Operands must be numbers."
                                  : "Operand must be a number."};
  }
}

struct Interpreter {
  auto operator()(LiteralExpression const& expr) -> Object {
    return expr.value_;
  }

  auto operator()(Box<GroupingExpression> const& expr) -> Object {
    return std::visit(*this, expr->expression_);
  }

  auto operator()(Box<UnaryExpression> const& expr) -> Object {
    Object const right = std::visit(*this, expr->right_);

    Token const& op = expr->op_;
    TokenType const& op_type = op.type_;

    if (op_type == TokenType::MINUS) {
      check_number_operand(op, right);
      return -std::get<double>(right);
    }

    auto const is_truthy = [](Object const& obj) {
      if (std::holds_alternative<std::monostate>(obj)) {
        return false;
      }

      if (std::holds_alternative<bool>(obj)) {
        return std::get<bool>(obj);
      }

      return true;
    };

    if (op_type == TokenType::BANG) {
      return !is_truthy(right);
    }

    return std::monostate{};
  }

  auto operator()(Box<BinaryExpression> const& expr) -> Object {
    Object const left = std::visit(*this, expr->left_);
    Object const right = std::visit(*this, expr->right_);

    Token const& op = expr->op_;
    TokenType const& op_type = op.type_;

    if (op_type == TokenType::MINUS) {
      check_number_operand(op, left, right);
      return std::get<double>(left) - std::get<double>(right);
    }
    if (op_type == TokenType::SLASH) {
      check_number_operand(op, left, right);
      return std::get<double>(left) / std::get<double>(right);
    }
    if (op_type == TokenType::STAR) {
      check_number_operand(op, left, right);
      return std::get<double>(left) * std::get<double>(right);
    }
    if (op_type == TokenType::PLUS) {
      if (std::holds_alternative<double>(left) &&
          std::holds_alternative<double>(right)) {
        return std::get<double>(left) + std::get<double>(right);
      }
      if (std::holds_alternative<std::string>(left) &&
          std::holds_alternative<std::string>(right)) {
        return std::get<std::string>(left) + std::get<std::string>(right);
      }
      throw RuntimeError{op, "Operands must be two numbers or two strings."};
    }
    if (op_type == TokenType::GREATER) {
      check_number_operand(op, left, right);
      return std::get<double>(left) > std::get<double>(right);
    }
    if (op_type == TokenType::GREATER_EQUAL) {
      check_number_operand(op, left, right);
      return std::get<double>(left) >= std::get<double>(right);
    }
    if (op_type == TokenType::LESS) {
      check_number_operand(op, left, right);
      return std::get<double>(left) < std::get<double>(right);
    }
    if (op_type == TokenType::LESS_EQUAL) {
      check_number_operand(op, left, right);
      return std::get<double>(left) <= std::get<double>(right);
    }
    if (op_type == TokenType::BANG_EQUAL) {
      return left != right;
    }
    if (op_type == TokenType::EQUAL_EQUAL) {
      return left == right;
    }
    // Unreachable
    return std::monostate{};
  }
};
}  // namespace

#endif