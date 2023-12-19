#ifndef LOX_SCANNER
#define LOX_SCANNER

#include <string>
#include <vector>

#include "token.h"
#include "utils/error.h"

namespace Scanner {
struct Error : LoxError {
  Error(std::size_t line, std::string const& message);

  std::size_t line_;
  std::string message_;

  auto report() const -> void final;
};

[[nodiscard]] auto scan_tokens(std::string const& contents)
    -> std::vector<Token const>;
}  // namespace Scanner

#endif
