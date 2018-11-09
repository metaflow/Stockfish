/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2018 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "bitboard.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "tt.h"
#include "uci.h"
#include "syzygy/tbprobe.h"

namespace PSQT {
  void init();
}

Square mirror(const Square s) {
  return make_square(file_of(s), Rank((int)RANK_8 - rank_of(s)));
}

const std::string PieceToChar("  NBRQK   NBRQK");
const std::string FileToChar("abcdefgh");
void record(int d, std::vector<Move>& h) {
  Position p;
  StateInfo s;
  p.set("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", false, &s, Threads.main());
  for (int i = 0; i < d; i++) {
    if (i % 2 == 0) std::cout << i + 1 << ". ";
    Move m = h[i];
    Square from = from_sq(m);
    Square to = to_sq(m);
    Piece piece = p.piece_on(from);
    std::cout << PieceToChar[piece]
              << FileToChar[file_of(from)] << (int)rank_of(from) + 1
              << FileToChar[file_of(to)] << (int)rank_of(to) + 1;
    if (type_of(m) == PROMOTION) {
      std::cout << '=' << PieceToChar[promotion_type(m)];
    }
    std::cout << ' ';
    StateInfo n;
    p.do_move(m, n);
  }
  std::cout << std::endl;
}

bool dfs(Position& p, int depth, std::unordered_map<Key, int>& visited,
         std::vector<Move>& h, std::unordered_set<PieceType>& found) {
  if (depth * 2 + 1 >= (int)h.size()) {
    return false;
  }
  MoveList<LEGAL> ms(p);
  for (const auto& m : ms) {
    StateInfo s;
    Square m_from = from_sq(m);
    Square m_from_m = mirror(m_from);
    Square m_to = to_sq(m);
    Square m_to_m = mirror(m_to);
    PieceType m_promo = promotion_type(m);
    PieceType m_piece = type_of(p.piece_on(m_from));
    bool check = p.gives_check(m);
    p.do_move(m, s, check);
    h[depth * 2] = m;
    MoveList<LEGAL> as(p);
    Move a(MOVE_NONE);
    for (const auto& x : as) {
      if (from_sq(x) == m_from_m &&
          to_sq(x) == m_to_m &&
          type_of(p.piece_on(from_sq(x))) == m_piece &&
          promotion_type(x) == m_promo) {
        a = x;
        break;
      }
    }
    if (a == MOVE_NONE) {
      if (check && as.size() == 0) {
        PieceType t = m_piece;
        if (type_of(m) == PROMOTION) t = NO_PIECE_TYPE;
        if (found.count(t) == 0) {
          found.emplace(t);
          std::cerr << p << std::endl;
          record(depth * 2 + 1, h);
          if (found.size() == 7) return true;
        }
      }
    } else {
      StateInfo s2;
      p.do_move(a, s2);
      h[depth * 2 + 1] = a;
      Key k = p.key();
      if (visited[k] < h.size() - depth) {
        visited[k] = depth;
        if (dfs(p, depth + 1, visited, h, found)) return true;
      }
      p.undo_move(a);
    }
    p.undo_move(m);
  }
  return false;
}

int main(int argc, char* argv[]) {
  std::cout << engine_info() << std::endl;

  // UCI::init(Options);
  PSQT::init();
  Bitboards::init();
  Position::init();
  Bitbases::init();
  Search::init();
  Pawns::init();
  Tablebases::init(Options["SyzygyPath"]); // After Bitboards are set
  Threads.set(Options["Threads"]);
  // Search::clear(); // After threads are up

  // UCI::loop(argc, argv);

  Threads.set(1);
  Position p;
  StateInfo state;
  p.set("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", false, &state, Threads.main());
  std::vector<Move> h(5, MOVE_NONE);
  std::unordered_set<PieceType> found;
  while (1) {
    h.emplace_back(MOVE_NONE);
    h.emplace_back(MOVE_NONE);
    std::unordered_map<Key, int> visited;
    if (dfs(p, 0, visited, h, found)) break;
  }
  return 0;
}
