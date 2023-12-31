#include <iostream>
#include <unordered_map>
#include <vector>

#include "./interpreter.hpp"
#include "./types/statement.hpp"
#include "./utils/error.hpp"

namespace Resolver {
struct Error : CompileTimeError {
  Error(std::size_t line, std::string const& message)
      : line_(line), message_(message) {}

  std::size_t line_;
  std::string message_;

  auto report() const -> void final {
    std::cerr << "[line " << line_ << "] Resolver error: " << message_ << '\n';
  }
};

auto error(std::size_t line, std::string const& message) -> Error {
  return Error{line, message};
}
}  // namespace Resolver

class NameResolver {
 public:
  NameResolver(std::unordered_map<Token, std::size_t>& resolution)
      : resolution_{resolution},
        scopes_{{}},  // TODO add global names here
        current_function_type_{FunctionType::NONE},
        current_class_type_{ClassType::NONE} {}

  auto resolve(Expression const& expr) -> void { std::visit(*this, expr); }

  auto resolve(Statement const& stmt) -> void { std::visit(*this, stmt); }

  auto resolve(std::vector<Statement> const& statements) -> void {
    for (auto const& statement : statements) {
      resolve(statement);
    }
  }

  auto operator()(std::monostate) -> void {}

  auto operator()(ExpressionStatement const& stmt) -> void {
    resolve(stmt.expression_);
  }

  auto operator()(PrintStatement const& stmt) -> void {
    resolve(stmt.expression_);
  }

  auto operator()(ReturnStatement const& stmt) -> void {
    if (current_function_type_ == FunctionType::NONE) {
      throw Resolver::error(stmt.keyword_.line_,
                            "Can't return from top-level code.");
    }

    resolve(stmt.value_);
  }

  auto operator()(VariableStatement const& stmt) -> void {
    declare(stmt.name_);
    resolve(stmt.initializer_);
    define(stmt.name_);
  }

  auto operator()(Box<BlockStatement> const& stmt) -> void {
    begin_scope();
    resolve(stmt->statements_);
    end_scope();
  }

  auto operator()(Box<FunctionStatement> const& stmt) -> void {
    declare(stmt->name_);
    define(stmt->name_);

    resolve_function(*stmt, FunctionType::FUNCTION);
  }

  auto operator()(Box<ClassStatement> const& stmt) -> void {
    ClassType const enclosing_class{current_class_type_};
    current_class_type_ = ClassType::CLASS;

    declare(stmt->name_);
    define(stmt->name_);

    begin_scope();
    scopes_.back()["this"] = true;

    for (Box<FunctionStatement> const& method : stmt->methods_) {
      FunctionType const declaration{FunctionType::METHOD};
      resolve_function(*method, declaration);
    }

    end_scope();

    current_class_type_ = enclosing_class;
  }

  auto operator()(Box<IfStatement> const& stmt) -> void {
    resolve(stmt->condition_);
    resolve(stmt->then_branch_);
    if (!std::holds_alternative<std::monostate>(stmt->else_branch_)) {
      resolve(stmt->else_branch_);
    }
  }

  auto operator()(Box<WhileStatement> const& stmt) -> void {
    resolve(stmt->condition_);
    resolve(stmt->body_);
  }

  auto operator()(LiteralExpression const& expr) -> void {}

  auto operator()(ThisExpression const& expr) -> void {
    if (current_class_type_ == ClassType::NONE) {
      throw Resolver::error(expr.keyword_.line_,
                            "Can't use 'this' outside of a class.");
    }

    resolve_local(expr.keyword_);
  }

  auto operator()(VariableExpression const& expr) -> void {
    if (!scopes_.empty()) {
      if (auto const found{scopes_.back().find(expr.name_.lexeme_)};
          found != scopes_.back().end() && found->second == false) {
        throw Resolver::error(
            expr.name_.line_,
            "Can't read local variable in its own initializer.");
      }
    }
    resolve_local(expr.name_);
  }

  auto operator()(Box<AssignmentExpression> const& expr) -> void {
    resolve(expr->value_);
    resolve_local(expr->name_);
  }

  auto operator()(Box<BinaryExpression> const& expr) -> void {
    resolve(expr->left_);
    resolve(expr->right_);
  }

  auto operator()(Box<CallExpression> const& expr) -> void {
    resolve(expr->callee_);

    for (Expression const& argument : expr->arguments_) {
      resolve(argument);
    }
  }

  auto operator()(Box<GetExpression> const& expr) -> void {
    resolve(expr->object_);
  }

  auto operator()(Box<GroupingExpression> const& expr) -> void {
    resolve(expr->expression_);
  }

  auto operator()(Box<LogicalExpression> const& expr) -> void {
    resolve(expr->left_);
    resolve(expr->right_);
  }

  auto operator()(Box<SetExpression> const& expr) -> void {
    resolve(expr->value_);
    resolve(expr->object_);
  }

  auto operator()(Box<UnaryExpression> const& expr) -> void {
    resolve(expr->right_);
  }

 private:
  enum class FunctionType { NONE, FUNCTION, METHOD };
  enum class ClassType { NONE, CLASS };

  auto begin_scope() -> void { scopes_.emplace_back(); }

  auto end_scope() -> void { scopes_.pop_back(); }

  auto declare(Token const& name) -> void {
    if (!scopes_.empty()) {
      auto& scope{scopes_.back()};
      if (scope.find(name.lexeme_) != scope.end()) {
        throw Resolver::error(name.line_,
                              "Already a variable with this name declared in "
                              "this scope.");
      }
      scopes_.back()[name.lexeme_] = false;
    }
  }

  auto define(Token const& name) -> void {
    if (!scopes_.empty()) {
      scopes_.back()[name.lexeme_] = true;
    }
  }

  auto resolve_local(Token const& name) -> void {
    for (auto scope = scopes_.crbegin(); scope != scopes_.crend(); ++scope) {
      if (scope->find(name.lexeme_) != scope->end()) {
        resolution_[name] = std::distance(scopes_.crbegin(), scope);
        return;
      }
    }
  }

  auto resolve_function(FunctionStatement const& stmt,
                        FunctionType function_type) -> void {
    FunctionType const enclosing_function{current_function_type_};
    current_function_type_ = function_type;

    begin_scope();

    for (Token const& param : stmt.params_) {
      declare(param);
      define(param);
    }
    resolve(stmt.body_);

    end_scope();

    current_function_type_ = enclosing_function;
  }

  std::vector<std::unordered_map<std::string, bool>> scopes_;
  FunctionType current_function_type_;
  ClassType current_class_type_;

  std::unordered_map<Token, std::size_t>& resolution_;
};

namespace Resolver {
auto resolve(std::vector<Statement> const& statements)
    -> std::unordered_map<Token, std::size_t> {
  std::unordered_map<Token, std::size_t> resolution;
  NameResolver resolver{resolution};

  for (Statement const& statement : statements) {
    std::visit(resolver, statement);
  }

  return resolution;
}
}  // namespace Resolver
