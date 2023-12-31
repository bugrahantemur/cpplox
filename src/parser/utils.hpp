#ifndef LOX_PARSER_UTILS
#define LOX_PARSER_UTILS

#include <vector>

#include "./cursor.hpp"
#include "./error.hpp"

namespace Parser::Utils {
template <typename T, typename F>
auto parse_parenthesized_list(Cursor& cursor, F const& f) -> std::vector<T> {
  cursor.take(TokenType::LEFT_PAREN);

  std::vector<T> list{};

  if (!cursor.match(TokenType::RIGHT_PAREN)) {
    list.push_back(f(cursor));
    while (cursor.match(TokenType::COMMA)) {
      cursor.take();
      list.push_back(f(cursor));
    }
  }

  if (list.size() >= 255) {
    error(cursor.peek(), "Can't have more than 255 constituents.").report();
  }

  cursor.take(TokenType::RIGHT_PAREN);

  return list;
}
}  // namespace Parser::Utils
#endif
