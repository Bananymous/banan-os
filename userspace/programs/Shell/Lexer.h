#pragma once

#include "Token.h"

BAN::ErrorOr<BAN::Vector<Token>> tokenize_string(BAN::StringView);
