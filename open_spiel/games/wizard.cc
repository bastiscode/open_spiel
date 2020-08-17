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

#include "wizard.h"

#include <algorithm>

#include "open_spiel/abseil-cpp/absl/strings/str_format.h"
#include "open_spiel/game_parameters.h"
#include "open_spiel/spiel_utils.h"

namespace open_spiel::wizard {
    namespace {

        const GameType kGameType{
                "wizard",
                "Wizard",
                GameType::Dynamics::kSequential,
                GameType::ChanceMode::kExplicitStochastic,
                GameType::Information::kImperfectInformation,
                GameType::Utility::kGeneralSum,
                GameType::RewardModel::kTerminal,
                kMaxPlayers,
                kMinPlayers,
                true,
                true,
                true,
                true,
                {{"players", GameParameter(kDefaultPlayers)},
                 {"round", GameParameter(kFirstRound)},
                 {"start_player", GameParameter(0)},
                 {"reward_mode", GameParameter(0)}},
                true,
                false};

        std::shared_ptr<const Game> Factory(const GameParameters &params) {
            return std::shared_ptr<const Game>(new WizardGame(params));
        }

        REGISTER_SPIEL_GAME(kGameType, Factory)

    }

    class WizardObserver : public Observer {
    public:
        explicit WizardObserver(IIGObservationType iig_obs_type) : Observer(true, true),
                                                                   iig_obs_type_(iig_obs_type) {}

        void WriteTensor(const State &observedState, int player, Allocator *allocator) const override {
            const auto &state = open_spiel::down_cast<const WizardState &>(observedState);
            SPIEL_CHECK_GE(player, 0);
            SPIEL_CHECK_LT(player, state.num_players_);

            int numHandFeatures = kNumSpecials + kNumColors * kMaxCardValue;

            {
                auto out = allocator->Get("player", {state.num_players_});
                out.at(player) = 1;
            }

            if (iig_obs_type_.private_info == PrivateInfoType::kSinglePlayer) {
                auto playerHand = state.r.hands[player];
                auto out = allocator->Get("private_hand", {numHandFeatures});
                for (const auto &card : playerHand) {
                    out.at(card.ToIdx()) += 1;
                }
            }

            // when private info for all players is supported
//                else if (iig_obs_type_.private_info == PrivateInfoType::kAllPlayers) {
//                    auto out = allocator->Get("private_hands", {state.num_players_, numHandFeatures});
//                    for (int p = 0; p < state.num_players_; p++) {
//                        auto playerHand = state.r.hands[p];
//                        for (const auto & card : playerHand) {
//                            out.at(p, card.ToIdx()) += 1;
//                        }
//                    }
//                }

            if (iig_obs_type_.public_info) {
                if (iig_obs_type_.perfect_recall) {
                    // information state
                    {
                        // round
                        auto out = allocator->Get("round", {1});
                        out.at(0) = (float) state.r.getRoundNr();
                    }
                    {
                        // move
                        auto out = allocator->Get("move", {1});
                        out.at(0) = (float) state.move_number_;
                    }
                    {
                        // trump
                        auto out = allocator->Get("trump", {state.num_players_});
                        auto trumpColor = state.r.getTrump().getColor();
                        if (trumpColor != kWhite) {
                            out.at(trumpColor) = 1;
                        }
                    }
                    {
                        // guessed tricks
                        auto out = allocator->Get("guessed_tricks", {state.num_players_});
                        auto guessedTricks = state.r.getGuessedTricks();
                        for (int i = 0; i < state.num_players_; i++) {
                            out.at(i) = guessedTricks[i];
                        }
                    }
                    {
                        // all played cards by each player
                        auto out = allocator->Get("playing_history",
                                                  {state.num_players_ * state.r.getRoundNr(), numHandFeatures});
                        auto playedCardsOnTable = state.r.getCardsPlayedOnTable();
                        auto playedCards = state.r.getCardsPlayed();
                        int idx = 0;
                        for (auto &card : playedCards) {
                            out.at(idx, card.ToIdx()) = 1;
                            idx++;
                        }
                        for (auto &card : playedCardsOnTable) {
                            out.at(idx, card.ToIdx()) = 1;
                            idx++;
                        }
                    }
                } else {
                    // observation state
                    {
                        // round
                        auto out = allocator->Get("round", {1});
                        out.at(0) = (float) state.r.getRoundNr();
                    }
                    {
                        // trump
                        auto out = allocator->Get("trump", {state.num_players_});
                        auto trumpColor = state.r.getTrump().getColor();
                        if (trumpColor != kWhite) {
                            out.at(trumpColor) = 1;
                        }
                    }
                    {
                        // guessed tricks
                        auto out = allocator->Get("guessed_tricks", {state.num_players_});
                        auto guessedTricks = state.r.getGuessedTricks();
                        for (int i = 0; i < state.num_players_; i++) {
                            out.at(i) = guessedTricks[i];
                        }
                    }
                    {
                        // tricks
                        auto out = allocator->Get("tricks", {state.num_players_});
                        auto tricks = state.r.getTricks();
                        for (int i = 0; i < state.num_players_; i++) {
                            out.at(i) = tricks[i];
                        }
                    }
                    {
                        // played cards in current trick by each player
                        auto out = allocator->Get("played_cards_on_table", {state.num_players_, numHandFeatures});
                        auto playedCards = state.r.getCardsPlayedOnTable();
                        for (int i = 0; i < playedCards.size(); i++) {
                            out.at(i, playedCards[i].ToIdx()) = 1;
                        }
                    }
                }
            }
        }

        [[nodiscard]] std::string StringFrom(const State &observedState, int player) const override {
            const auto &state = open_spiel::down_cast<const WizardState &>(observedState);
            SPIEL_CHECK_GE(player, 0);
            SPIEL_CHECK_LT(player, state.num_players_);
            if (state.r.getGameState() == kDealing) {
                return "dealing cards";
            }

            auto const cardStringFormatter = CardStringFormatter();
            auto const isGuessing = state.r.getGameState() == kGuessing;
            auto const trump = state.r.getTrump();
            auto const legalActions = state.r.GetLegalActions(player);
            auto const cardsPlayed = state.r.getCardsPlayed();
            auto const playedBy = state.r.getPlayedBy();
            auto const playedByOnTable = state.r.getPlayedByOnTable();
            auto const cardsPlayedOnTable = state.r.getCardsPlayedOnTable();
            auto const playerHand = state.r.hands[player];
            auto const guessedTricks = state.r.getGuessedTricks();
            auto const tricks = state.r.getTricks();
            auto string = absl::StrCat(
                    absl::StrFormat("playerNr\t%d\n", player),
                    absl::StrFormat("currentPlayer\t%d\n", state.r.getTurn()),
                    absl::StrFormat("round\t%d\n", state.r.getRoundNr()),
                    absl::StrFormat("numPlayers\t%d\n", state.num_players_),
                    absl::StrFormat("guessedTricks\t%s\n", absl::StrJoin(guessedTricks, ",")),
                    absl::StrFormat("tricks\t%s\n", absl::StrJoin(tricks, ",")),
                    absl::StrFormat("gamePhase\t%s\n", isGuessing ? "guessing"
                                                                  : "tricking"),
                    absl::StrFormat("cardsPlayedOnTable\t%s\n", absl::StrJoin(cardsPlayedOnTable.begin(),
                                                                              cardsPlayedOnTable.end(),
                                                                              ",",
                                                                              cardStringFormatter)),
                    absl::StrFormat("playedByOnTable\t%s\n", absl::StrJoin(playedByOnTable.begin(),
                                                                           playedByOnTable.end(),
                                                                           ",")),
                    absl::StrFormat("hand\t%s\n", absl::StrJoin(playerHand.begin(),
                                                                playerHand.end(),
                                                                ",",
                                                                cardStringFormatter)),
                    absl::StrFormat("trump\t%s\n", trump.ToStr()),
                    absl::StrFormat("legalActions\t%s\n", absl::StrJoin(legalActions.begin(),
                                                                        legalActions.end(),
                                                                        ","))
            );
            if (iig_obs_type_.public_info) {
                if (iig_obs_type_.perfect_recall) {
                    // information state
                    // extend observed information by history of all cards played
                    absl::StrAppend(&string,
                                    absl::StrFormat("cardsPlayed\t%s\n", absl::StrJoin(cardsPlayed.begin(),
                                                                                       cardsPlayed.end(),
                                                                                       ",",
                                                                                       cardStringFormatter)),
                                    absl::StrFormat("playedBy\t%s\n", absl::StrJoin(playedBy.begin(),
                                                                                    playedBy.end(),
                                                                                    ",")));
                }
            }
            return string;
        }

    private:
        IIGObservationType iig_obs_type_;
    };

    std::vector<int> WizardGame::InformationStateTensorShape() const {
        // One-hot for whose turn it is (n)
        // Encoding of players hand (numHandFeatures)
        // Round nr (1)
        // Move number (1)
        // Encoding of trump (numColors)
        // Guessed tricks (n)
        // Card playing history (n * r * numHandFeatures)
        // n + numHandFeatures + 1 + 1 + numColors + n * (r+1) + n * r * numHandFeatures =
        // 2 * n + numHandFeatures + n * r * (numHandFeatures + 1) + numColors + 2
        int numHandFeatures = kNumSpecials + kNumColors * kMaxCardValue;
        return {2 * num_players_ + numHandFeatures + (num_players_ * round_nr_) * (numHandFeatures + 1) +
                kNumColors + 2};
    }

    std::vector<int> WizardGame::ObservationTensorShape() const {
        // One-hot for whose turn it is (n)
        // Encoding of players hand (numHandFeatures)
        // Round nr (1)
        // Encoding of trump (numColors)
        // Current tricks and guessed tricks for each player (n + n)
        // Played card in current trick by each player (n * numHandFeatures)
        // n + numHandFeatures + 1 + numColors + n + n + n * numHandFeatures =
        // n * (numHandFeatures + 3) + numHandFeatures + numColors + 1
        int numHandFeatures = kNumSpecials + kNumColors * kMaxCardValue;
        return {num_players_ * (numHandFeatures + 3) + numHandFeatures + kNumColors + 1};
    }

    WizardGame::WizardGame(const GameParameters &params)
            : Game(kGameType, params) {
        defaultObserver_ = std::make_shared<WizardObserver>(kDefaultObsType);
        infoStateObserver_ = std::make_shared<WizardObserver>(kInfoStateObsType);

        auto const numPlayers = ParameterValue<int>("players");
        auto const rewardMode = (RewardMode) ParameterValue<int>("reward_mode");
        auto const startPlayer = ParameterValue<int>("start_player");
        auto const roundNr = ParameterValue<int>("round");

        this->num_players_ = numPlayers;
        this->reward_mode_ = rewardMode;
        this->start_player_ = startPlayer;
        this->round_nr_ = roundNr;
    }

    int WizardGame::NumDistinctActions() const {
        return kNumCardActions + (kDeckSize / this->NumPlayers()) + 1;
    }

    std::unique_ptr<State> WizardGame::NewInitialState() const {
        return std::unique_ptr<State>(new WizardState(shared_from_this(), reward_mode_, start_player_, round_nr_));
    }

    int WizardGame::NumPlayers() const {
        return num_players_;
    }

    double WizardGame::MinUtility() const {
        if (this->reward_mode_ == kBinaryReward) {
            return -1;
        } else {
            auto minUtility = 0;
            for (int i = 1; i <= kDeckSize / this->NumPlayers(); i++) {
                minUtility += i * -10;
            }
            return minUtility;
        }
    }

    double WizardGame::MaxUtility() const {
        if (this->reward_mode_ == kBinaryReward) {
            return 1;
        } else {
            auto maxUtility = 0;
            for (int i = 1; i <= kDeckSize / this->NumPlayers(); i++) {
                maxUtility += 20 + i * 10;
            }
            return maxUtility;
        }
    }

    std::shared_ptr<const Game> WizardGame::Clone() const {
        return std::shared_ptr<const Game>(new WizardGame(*this));
    }

    int WizardGame::MaxGameLength() const {
        return this->num_players_ * this->round_nr_ + this->num_players_;
    }

    int WizardGame::MaxChanceOutcomes() const {
        return kDistinctCards;
    }

    std::shared_ptr<Observer>
    WizardGame::MakeObserver(absl::optional<IIGObservationType> iig_obs_type, const GameParameters &params) const {
        if (!params.empty()) SpielFatalError("Observation params not supported");
        return std::make_shared<WizardObserver>(iig_obs_type.value_or(kDefaultObsType));
    }

    WizardState::WizardState(const std::shared_ptr<const Game> &game, RewardMode rewardMode, int startPlayer,
                             int roundNr) : State(game) {
        num_distinct_actions_ = game->NumDistinctActions();
        num_players_ = game->NumPlayers();
        reward_mode_ = rewardMode;
        this->r = Round(num_players_, roundNr, startPlayer, rewardMode);
    }

    Player WizardState::CurrentPlayer() const {
        if (IsTerminal()) {
            return kTerminalPlayerId;
        } else {
            return this->r.getTurn();
        }
    }

    std::vector<Action> WizardState::LegalActions() const {
        if (IsTerminal()) {
            return {};
        }
        if (IsChanceNode()) {
            std::vector<Action> legalActions;
            auto const cardCounts = this->r.getDeck().getCardCounts();
            for (int i = 0; i < cardCounts.size(); i++) {
                if (cardCounts[i] > 0) {
                    legalActions.emplace_back(i);
                }
            }
            if (legalActions.empty()) {
                // can only happen in last round if all cards are dealt
                // then add noob which gets played as trump
                legalActions.emplace_back(0);
            }
            return legalActions;
        }
        auto const legalActions = this->r.GetLegalActions();
        std::vector<Action> actions(legalActions.begin(), legalActions.end());
        return actions;
    }

    std::string WizardState::ActionToString(Player player, Action action_id) const {
        if (this->r.getGameState() == kDealing) {
            return Card(action_id).ToStr();
        } else if (this->r.getGameState() == kGuessing) {
            return std::to_string(action_id);
        } else {
            return Card((int) action_id - this->r.getNumGuessActions()).ToStr();
        }
    }

    std::string WizardState::ToString() const {
        std::string string;
        for (auto i : history_) {
            if (!string.empty()) absl::StrAppend(&string, ",");
            absl::StrAppend(&string, absl::StrFormat("(%d, %d)", i.player, i.action));
        }
    }

    bool WizardState::IsTerminal() const {
        return this->r.IsFinal();
    }

    std::vector<double> WizardState::Returns() const {
        return this->r.GetRewards(this->reward_mode_);
    }

    void WizardState::DoApplyAction(Action action_id) {
        history_by_.emplace_back(CurrentPlayer());
        if (this->r.getGameState() == kDealing) {
            this->r.DealCard(action_id);
        } else if (this->r.getGameState() == kGuessing) {
            this->r.GuessTricks(action_id);
        } else {
            // action is a card play action, resolve correct card to play in PlayCard
            auto const finishedOneTrick = this->r.PlayCard(action_id);
            // if  we are finished with one round of tricking (every player played one card),
            // then we update the tricks and thereby go into the next round
            if (finishedOneTrick) {
                this->r.UpdateTricks();
            }
        }
    }

    std::unique_ptr<State> WizardState::Clone() const {
        return std::unique_ptr<State>(new WizardState(*this));
    }

    std::unique_ptr<State> WizardState::ResampleFromInfostate(int player_id, std::function<double()> rng) const {
        // get a copy of the current state
        std::unique_ptr<State> clone = std::make_unique<WizardState>(game_,
                                                                     this->r.getRewardMode(),
                                                                     this->r.getStartPlayer(),
                                                                     this->r.getRoundNr());

        // only thing that is private from us are the others players hands.
        // so to resample the info state we just have to randomly replace remaining cards in the others players
        // hands with cards from the deck or from all other players hands.
        // To do this we create a pool of all cards that were not seen yet.
        // Then we randomly replace the cards in the players hands which cards from this pool.

        // get cards which are still in deck
        auto cardPool = this->r.getDeck().getCardCounts();
        // add cards from other players hands
        for (int i = 0; i < this->num_players_; i++) {
            if (i == player_id) continue;
            auto const hand = this->r.hands[i];
            for (auto const &card : hand) {
                cardPool[card.ToIdx()] += 1;
            }
        }

        int currentDealTo = this->r.getStartPlayer();
        Action previousActionTaken;
        std::default_random_engine rand(std::chrono::high_resolution_clock::now().time_since_epoch().count());

        // card dealing

        // must deal all cards which were already played, because now they are known information
        auto cardsPlayed = this->r.getCardsPlayed();
        auto const cardsPlayedOnTable = this->r.getCardsPlayedOnTable();
        auto playedBy = this->r.getPlayedBy();
        auto const playedByOnTable = this->r.getPlayedByOnTable();
        std::map<int, std::vector<Card>> cardsPlayedBy;
        for (int i = 0; i < playedBy.size(); i++) {
            cardsPlayedBy[playedBy[i]].emplace_back(cardsPlayed[i]);
        }
        for (int i = 0; i < playedByOnTable.size(); i++) {
            cardsPlayedBy[playedByOnTable[i]].emplace_back(cardsPlayedOnTable[i]);
        }
        for (int i = 0; i < this->r.getCardsDealt(); i++) {
            SPIEL_CHECK_TRUE(clone->IsChanceNode());
            previousActionTaken = history_[i].action;
            if (currentDealTo == player_id) {
                // deal the same card as previous
                clone->ApplyAction(previousActionTaken);
            } else if (!cardsPlayedBy[currentDealTo].empty()) {
                // check if player played a card already, then this card has to be dealt
                auto const idx = cardsPlayedBy[currentDealTo].back().ToIdx();
                clone->ApplyAction(idx);
                cardsPlayedBy[currentDealTo].pop_back();
            } else {
                // deal random card from card pool (cards in deck + unseen cards)
                int idx;
                bool validCardIdx = false;
                while (!validCardIdx) {
                    std::discrete_distribution<int> distribution(cardPool.begin(), cardPool.end());
                    idx = distribution(rand);
                    if (cardPool[idx] > 0) {
                        // found valid card
                        validCardIdx = true;
                    }
                }
                // remove the card from the card pool
                cardPool[idx]--;
                clone->ApplyAction(idx);
            }
            currentDealTo++;
            if (currentDealTo >= this->num_players_) {
                currentDealTo = 0;
            }
        }

        // trump
        auto const trumpDealt = history_.size() >= this->num_players_ * this->r.getRoundNr() + 1;
        auto const trumpIdx = this->num_players_ * this->r.getRoundNr();
        if (trumpDealt) {
            // trump was dealt
            SPIEL_CHECK_TRUE(this->r.getCardsDealt() == trumpIdx);
            clone->ApplyAction(history_[trumpIdx].action);
        }
        // guessing
        auto const guessingLower = trumpIdx + 1;
        auto const guessingUpper = std::min((int) history_.size(),
                                            guessingLower + this->num_players_);
        for (int i = guessingLower; i < guessingUpper; i++) {
            // guess the same as previous
            clone->ApplyAction(history_[i].action);
        }

        //tricking
        auto const trickingLower = guessingUpper;
        auto const trickingUpper = history_.size();
        for (int i = trickingLower; i < trickingUpper; i++) {
            // trick the same as previous
            clone->ApplyAction(history_[i].action);
        }

        return clone;
    }

    ActionsAndProbs WizardState::ChanceOutcomes() const {
        SPIEL_CHECK_TRUE(this->IsChanceNode());
        std::vector<std::pair<Action, double>> outcomes;
        auto const cardsInDeck = this->r.getDeck().getCardsInDeck();
        if (cardsInDeck == 0) {
            // can only happen if we are in the final round, because there is no trump
            // then return noob as trump
            return {{0, 1}};
        }

        auto const cardCounts = this->r.getDeck().getCardCounts();
        for (int i = 0; i < cardCounts.size(); i++) {
            // if the card is still in the deck
            if (cardCounts[i] > 0) {
                outcomes.emplace_back(std::pair(i, (double) cardCounts[i] / (double) cardsInDeck));
            }
        }
        return outcomes;
    }

    void WizardState::InformationStateTensor(Player player, absl::Span<float> values) const {
        ContiguousAllocator allocator(values);
        const auto &game = open_spiel::down_cast<const WizardGame &>(*game_);
        game.infoStateObserver_->WriteTensor(*this, player, &allocator);
    }

    void WizardState::ObservationTensor(Player player, absl::Span<float> values) const {
        ContiguousAllocator allocator(values);
        const auto &game = open_spiel::down_cast<const WizardGame &>(*game_);
        game.defaultObserver_->WriteTensor(*this, player, &allocator);
    }

    std::string WizardState::InformationStateString(Player player) const {
        const auto &game = open_spiel::down_cast<const WizardGame &>(*game_);
        return game.infoStateObserver_->StringFrom(*this, player);
    }

    std::string WizardState::ObservationString(Player player) const {
        const auto &game = open_spiel::down_cast<const WizardGame &>(*game_);
        return game.defaultObserver_->StringFrom(*this, player);
    }
}
