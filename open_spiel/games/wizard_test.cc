// Copyright 2019 DeepMind Technologies Ltd. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "open_spiel/spiel.h"
#include "open_spiel/tests/basic_tests.h"
#include "wizard.h"

namespace open_spiel {
    namespace wizard {
        namespace {

            namespace testing = open_spiel::testing;

            void BasicWizardTests() {
                testing::LoadGameTest("wizard");
                testing::ChanceOutcomesTest(*LoadGame("wizard"));
                for (Player players = 3; players <= 6; players++) {
                    for (int round = 1; round <= wizard::kDeckSize / players; round++) {
                        testing::RandomSimTest(
                                *LoadGame("wizard", {{"players",      GameParameter(players)},
                                                     {"round",        GameParameter(round)},
                                                     {"start_player", GameParameter(0)},
                                                     {"reward_mode",  GameParameter(0)}}
                                ), 10);
                        testing::ResampleInfostateTest(
                                *LoadGame("wizard", {{"players",      GameParameter(players)},
                                                     {"round",        GameParameter(round)},
                                                     {"start_player", GameParameter(0)},
                                                     {"reward_mode",  GameParameter(0)}}
                                ), 10);
                    }
                }
            }

        }  // namespace
    }  // namespace leduc_poker
}  // namespace open_spiel

int main(int argc, char **argv) { open_spiel::wizard::BasicWizardTests(); }
