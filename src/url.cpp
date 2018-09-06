// Copyright 2018 Glyn Matthews.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <algorithm>
#include <cassert>
#include <functional>
#include <locale>
#include <vector>
#include <cassert>
#include "skyr/url.hpp"
#include "skyr/url_parse.hpp"
#include "skyr/url_serialize.hpp"
#include "skyr/percent_encode.hpp"
#include "url_schemes.hpp"

namespace skyr {
url::url()
  : url_()
  , href_()
  , view_(href_) {}

url::url(url_record &&input) noexcept
  : url_(input)
  , href_(serialize(url_))
  , view_(href_)
  , parameters_(url_) {}

void url::swap(url &other) noexcept {
  using std::swap;
  swap(url_, other.url_);
  swap(href_, other.href_);
  view_ = string_view(href_);
  other.view_ = string_view(other.href_);
}

void url::initialize(std::string &&input, optional<url_record> base) {
  auto parsed_url = parse(input, base);
  if (!parsed_url) {
    throw url_parse_error(parsed_url.error());
  }
  update_record(std::move(parsed_url.value()));
}


void url::update_record(url_record &&record) {
  url_ = record;
  href_ = serialize(url_);
  view_ = string_view(href_);
  parameters_ = url_search_parameters(url_);
}

std::string url::href() const {
  return href_;
}

expected<void, std::error_code> url::set_href(std::string href) {
  auto new_url = details::basic_parse(href);
  if (!new_url) {
    return make_unexpected(std::move(new_url.error()));
  }

  update_record(std::move(new_url.value()));
  return {};
}

std::string url::to_json() const {
  return href_;
}

std::string url::protocol() const { return url_.scheme + ":"; }

expected<void, std::error_code> url::set_protocol(std::string protocol) {
  auto new_url = details::basic_parse(
      protocol + ":", nullopt, url_, url_parse_state::scheme_start);
  if (!new_url) {
    return make_unexpected(std::move(new_url.error()));
  }

  update_record(std::move(new_url.value()));
  return {};
}

std::string url::username() const { return url_.username; }

expected<void, std::error_code> url::set_username(std::string username) {
  if (url_.cannot_have_a_username_password_or_port()) {
    return make_unexpected(make_error_code(url_parse_errc::cannot_have_a_username_password_or_port));
  }

  auto new_url = url_;

  new_url.username.clear();
  for (auto c : username) {
    auto pct_encoded = percent_encode_byte(static_cast<std::byte>(c), userinfo_set());
    new_url.username += pct_encoded;
  }

  update_record(std::move(new_url));
  return {};
}

std::string url::password() const { return url_.password; }

expected<void, std::error_code> url::set_password(std::string password) {
  if (url_.cannot_have_a_username_password_or_port()) {
    return make_unexpected(make_error_code(url_parse_errc::cannot_have_a_username_password_or_port));
  }

  auto new_url = url_;

  new_url.password.clear();
  for (auto c : password) {
    auto pct_encoded = percent_encode_byte(static_cast<std::byte>(c), userinfo_set());
    new_url.password += pct_encoded;
  }

  update_record(std::move(new_url));
  return {};
}

std::string url::host() const {
  if (!url_.host) {
    return {};
  }

  if (!url_.port) {
    return url_.host.value();
  }

  return url_.host.value() + ":" + std::to_string(url_.port.value());
}

expected<void, std::error_code> url::set_host(std::string host) {
  if (url_.cannot_be_a_base_url) {
    return make_unexpected(make_error_code(url_parse_errc::cannot_be_a_base_url));
  }

  auto new_url = details::basic_parse(
      host, nullopt, url_, url_parse_state::host);
  if (!new_url) {
    return make_unexpected(std::move(new_url.error()));
  }

  update_record(std::move(new_url.value()));
  return {};
}

std::string url::hostname() const {
  if (!url_.host) {
    return {};
  }

  return url_.host.value();
}

expected<void, std::error_code> url::set_hostname(std::string hostname) {
  if (url_.cannot_be_a_base_url) {
    return make_unexpected(make_error_code(url_parse_errc::cannot_be_a_base_url));
  }

  auto new_url = details::basic_parse(
      hostname, nullopt, url_, url_parse_state::hostname);
  if (!new_url) {
    return make_unexpected(std::move(new_url.error()));
  }

  update_record(std::move(new_url.value()));
  return {};
}

std::string url::port() const {
  if (!url_.port) {
    return {};
  }

  return std::to_string(url_.port.value());
}

expected<void, std::error_code> url::set_port(std::string port) {
  if (url_.cannot_have_a_username_password_or_port()) {
    return make_unexpected(make_error_code(url_parse_errc::cannot_have_a_username_password_or_port));
  }

  if (port.empty()) {
    auto new_url = url_;
    new_url.port = nullopt;
    update_record(std::move(new_url));
  }
  else {
    auto new_url = details::basic_parse(
        port, nullopt, url_, url_parse_state::port);
    if (!new_url) {
      return make_unexpected(std::move(new_url.error()));
    }
    update_record(std::move(new_url.value()));
  }

  return {};
}

expected<void, std::error_code> url::set_port(std::uint16_t port) {
  return set_port(std::to_string(port));
}

std::string url::pathname() const {
  if (url_.cannot_be_a_base_url) {
    return url_.path.front();
  }

  if (url_.path.empty()) {
    return {};
  }

  auto pathname = std::string("/");
  for (const auto &segment : url_.path) {
    pathname += segment;
    pathname += "/";
  }
  return pathname.substr(0, pathname.length() - 1);
}

expected<void, std::error_code> url::set_pathname(std::string pathname) {
  if (url_.cannot_be_a_base_url) {
    return make_unexpected(make_error_code(url_parse_errc::cannot_be_a_base_url));
  }

  url_.path.clear();
  auto new_url = details::basic_parse(
      pathname, nullopt, url_, url_parse_state::path_start);
  if (!new_url) {
    return make_unexpected(std::move(new_url.error()));
  }
  update_record(std::move(new_url.value()));
  return {};
}

std::string url::search() const {
  if (!url_.query || url_.query.value().empty()) {
    return {};
  }

  return "?" + url_.query.value();
}

expected<void, std::error_code> url::set_search(std::string search) {
  auto url = url_;
  if (search.empty()) {
    url.query = nullopt;
    update_record(std::move(url));
    return {};
  }

  auto input = search;
  if (input.front() == '?') {
    auto first = std::begin(input), last = std::end(input);
    input.assign(first + 1, last);
  }

  url_.query = "";
  auto new_url = details::basic_parse(
      input, nullopt, url_, url_parse_state::query);
  if (!new_url) {
    return make_unexpected(std::move(new_url.error()));
  }
  update_record(std::move(new_url.value()));
  return {};
}

url_search_parameters &url::search_parameters() {
  return parameters_;
}

std::string url::hash() const {
  if (!url_.fragment || url_.fragment.value().empty()) {
    return {};
  }

  return "#" + url_.fragment.value();
}

expected<void, std::error_code> url::set_hash(std::string hash) {
  if (hash.empty()) {
    url_.fragment = nullopt;
    update_record(std::move(url_));
    return {};
  }

  auto input = hash;
  if (input.front() == '#') {
    auto first = std::begin(input), last = std::end(input);
    input.assign(first + 1, last);
  }

  url_.fragment = "";
  auto new_url = details::basic_parse(
      input, nullopt, url_, url_parse_state::fragment);
  if (!new_url) {
    return make_unexpected(std::move(new_url.error()));
  }
  update_record(std::move(new_url.value()));
  return {};
}

url_record url::record() const {
  return url_;
}

bool url::is_special() const noexcept { return url_.is_special(); }

bool url::validation_error() const noexcept { return false; }

optional<std::uint16_t> url::default_port(const std::string &scheme) noexcept {
  return details::default_port(string_view(scheme));
}

void url::clear() {
  update_record(url_record{});
}

const char *url::c_str() const noexcept {
  return href_.c_str();
}

url::operator url::string_type() const {
  return href_;
}

std::string url::string() const {
  return href_;
}

std::string url::u8string() const {
  return href_;
}

std::wstring url::wstring() const {
  auto result = wstring_from_bytes(view_);
  return result? result.value() : std::wstring();
}

std::u16string url::u16string() const {
  auto result = utf16_from_bytes(view_);
  return result? result.value() : std::u16string();
}

std::u32string url::u32string() const {
  auto result = utf32_from_bytes(view_);
  return result? result.value() : std::u32string();
}

void swap(url &lhs, url &rhs) noexcept {
  lhs.swap(rhs);
}

namespace details {
expected<url, std::error_code> make_url(
    std::string &&input,
    optional<url_record> base) {
  auto parsed_url = parse(std::move(input), std::move(base));
  if (!parsed_url) {
    return make_unexpected(std::move(parsed_url.error()));
  }
  return url(std::move(parsed_url.value()));
}
}  // namespace details
}  // namespace skyr
