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

// A generalized version of a Leduc poker, a simple but non-trivial poker game
// described in http://poker.cs.ualberta.ca/publications/UAI05.pdf .
//
// Taken verbatim from the linked paper above: "In Leduc hold'em, the deck
// consists of two suits with three cards in each suit. There are two rounds.
// In the first round a single private card is dealt to each player. In the
// second round a single board card is revealed. There is a two-bet maximum,
// with raise amounts of 2 and 4 in the first and second round, respectively.
// Both players start the first round with 1 already in the pot.
//
// So the maximin sequence is of the form:
// private card player 0, private card player 1, [bets], public card, [bets]
// Parameters:
//     "players"       int    number of players               (default = 2)

#ifndef THIRD_PARTY_OPEN_SPIEL_GAMES_WIZARD_H_
#define THIRD_PARTY_OPEN_SPIEL_GAMES_WIZARD_H_

#include <array>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "open_spiel/spiel.h"
#include "open_spiel/abseil-cpp/absl/strings/str_format.h"

namespace open_spiel::wizard {

// Default parameters.

// TODO(b/127425075): Use std::optional instead of sentinel values once absl is
// added as a dependency.
    inline constexpr int kDefaultPlayers = 4;
    inline constexpr int kFirstRound = 1;
    inline constexpr int kMinPlayers = 3;
    inline constexpr int kMaxPlayers = 6;

    inline constexpr int kNumColors = 4;  // number of colors (excluding white, because its a special color)
    inline constexpr int kNumSpecials = 2; // number of distinct special cards
    inline constexpr int kDeckSize = 60; // number of cards in deck
    inline constexpr int kDistinctCards = 54;
    inline constexpr int kMinCardValue = 1; // min value of a normal card
    inline constexpr int kMaxCardValue = 13; // max value of a normal card
    inline constexpr int kNumCardActions = kNumColors * kMaxCardValue + kNumSpecials; // number of cards i can play

    class WizardGame;

    enum GameState {
        kGuessing = 0, kTricking = 1, kDealing = 2
    };

    enum Colors {
        kBlue = 0, kRed = 1, kGreen = 2, kYellow = 3, kWhite = 4
    };

    enum SpecialCardValues {
        kNoobValue = 0, kWizardValue = 14
    };

    enum RewardMode {
        kNormalReward = 0, kBinaryReward = 1
    };

    class Card {
    public:
        Card() = default;

        Card(int color, int value) {
            this->color = color;
            this->value = value;
        }

        explicit Card(std::string s) {
            s = s.substr(1, s.size() - 2);
            auto const colorString = s.at(0);
            switch (colorString) {
                case 'B':
                    this->color = kBlue;
                    break;
                case 'R':
                    this->color = kRed;
                    break;
                case 'G':
                    this->color = kGreen;
                    break;
                case 'Y':
                    this->color = kYellow;
                    break;
                case 'W':
                    this->color = kWhite;
                    break;
                default:
                    throw std::invalid_argument(
                            absl::StrFormat("Error creating card with color %c", colorString));
            }
            int cardValue = std::stoi(s.substr(1, std::string::npos));
            if (this->color == kWhite && cardValue != kNoobValue && cardValue != kWizardValue) {
                throw std::invalid_argument(absl::StrFormat(
                        "Cannot create wizard or noob with value %d", cardValue
                ));
            } else if (this->color != kWhite && (cardValue < kMinCardValue || cardValue > kMaxCardValue)) {
                throw std::invalid_argument(absl::StrFormat(
                        "Cannot create card with value %d and color %c", cardValue, colorString
                ));
            }
            this->value = cardValue;
        }

        bool operator==(const Card &other) const {
            return (this->color == other.getColor() && this->value == other.getValue());
        }

        // for sorting cards
        bool operator<(const Card &other) const {
            if (this->color < other.color) {
                return true;
            } else if (this->color > other.color) {
                return false;
            } else {
                return this->value < other.value;
            }
        }

        explicit Card(int idx) {
            if (idx == 0) {
                this->color = kWhite;
                this->value = kNoobValue;
            } else if (idx == 1) {
                this->color = kWhite;
                this->value = kWizardValue;
            } else {
                idx = idx - kNumSpecials;
                this->color = floor((double) idx / kMaxCardValue);
                this->value = (idx % kMaxCardValue) + 1;

            }
        }

        [[nodiscard]] int ToIdx() const {
            if (this->isNoob()) {
                return 0;
            } else if (this->isWizard()) {
                return 1;
            } else {
                return kMaxCardValue * this->getColor() + this->getValue() - 1 + kNumSpecials;
            }
        }

        [[nodiscard]] std::string ToStr() const {
            std::string s = "[";
            switch (this->color) {
                case kBlue:
                    s += "B";
                    break;
                case kRed:
                    s += "R";
                    break;
                case kGreen:
                    s += "G";
                    break;
                case kYellow:
                    s += "Y";
                    break;
                case kWhite:
                    s += "W";
                    break;
            }
            auto rv = s + std::to_string(this->value) + "]";
            return rv;
        }

        [[nodiscard]] int getColor() const {
            return color;
        }

        [[nodiscard]] int getValue() const {
            return value;
        }

        [[nodiscard]] bool isWizard() const {
            return this->color == kWhite && this->value == 14;
        }

        [[nodiscard]] bool isNoob() const {
            return this->color == kWhite && this->value == 0;
        }

        [[nodiscard]] bool isTrump(Colors trumpColor) const {
            return this->color == trumpColor && trumpColor != kWhite;
        }

        int Compare(Card &other, Colors trumpColor) const {

            if (this->isWizard()) {
                return 1;
            } else if (other.isWizard()) {
                return -1;
            } else if (this->isNoob() && !other.isNoob()) {
                return -1;
            } else if (this->isTrump(trumpColor) && !other.isTrump(trumpColor)) {
                return 1;
            } else if (!this->isTrump(trumpColor) && other.isTrump(trumpColor)) {
                return -1;
            } else if (this->color != other.color) {
                return 1;
            } else if (this->value >= other.value) {
                return 1;
            } else {
                return -1;
            }
        }

    private:
        int color;

        int value;
    };

    struct CardStringFormatter {
        void operator()(std::string *out, Card card) const {
            out->append(card.ToStr());
        }
    };

    struct CardActionFormatter {
        explicit CardActionFormatter(int numGuessActions) {
            this->numGuessActions = numGuessActions;
        }

        int numGuessActions;

        void operator()(std::string *out, Action actionId) const {
            auto const idx = (int) actionId - this->numGuessActions;
            out->append(Card(idx).ToStr());
        }
    };

    class Deck {
    public:
        Deck() {
            this->cardCounts = std::vector<int>(kDistinctCards, 0);

            // add special cards in the first 8 indices
            for (int i = 0; i < 4; i++) {
                this->cardCounts[0] += 1;
                this->cardCounts[1] += 1;
                this->cardsInDeck += 2;
            }
//
//                // add normal cards after the first 8 indices
            for (int i = 0; i < kNumColors; i++) {
                for (int j = kMinCardValue; j <= kMaxCardValue; j++) {
                    auto const idx = kMaxCardValue * i + j - 1 + kNumSpecials;
                    this->cardCounts[idx] = 1;
                    this->cardsInDeck += 1;
                }
            }

            assert(this->cardsInDeck == kDeckSize);
        }

        Card DealCard(int cardIndex) {
            if (this->cardCounts[cardIndex] == 0) {
                throw std::invalid_argument(
                        absl::StrFormat("Cannot deal card %s because all of its kind were already dealt",
                                        Card(cardIndex).ToStr()));
            }
            this->cardCounts[cardIndex]--;
            this->cardsInDeck--;
            return Card(cardIndex);
        }

        [[nodiscard]] const std::vector<int> &getCardCounts() const {
            return cardCounts;
        }

        [[nodiscard]] int getCardsInDeck() const {
            return cardsInDeck;
        }

    private:
        std::vector<int> cardCounts;
        int cardsInDeck{};
    };


    class Round {
    public:
        Round() = default;

        Round(int numPlayers, int roundNr, int startPlayer, RewardMode rewardMode) {
            this->gameState = kDealing;
            this->numPlayers = numPlayers;
            this->startPlayer = startPlayer;
            this->dealTo = startPlayer;
            this->turn = kChancePlayerId;
            this->roundNr = roundNr;
            this->guessedTricks = std::vector<int>(this->numPlayers, 0);
            this->tricks = std::vector<int>(this->numPlayers, 0);
            this->numGuessActions = (kDeckSize / this->numPlayers) + 1;
            this->numActions = this->numGuessActions + kNumCardActions;
            this->cardsDealt = 0;
            this->final = false;
            this->numTricks = 0;
            this->rewardMode = rewardMode;

            this->hands = std::vector<std::vector<Card>>(this->numPlayers);
        }

        [[nodiscard]] std::vector<int> GetLegalActions() const {
            return this->GetLegalActions(this->turn);
        }

        [[nodiscard]] std::vector<int> GetLegalActions(int playerNr) const {
            auto legalGuessActions = this->GetLegalGuessActions(playerNr);
            auto const legalCardActions = this->GetLegalCardActions(playerNr);
            // join actions
            legalGuessActions.insert(legalGuessActions.end(), legalCardActions.begin(), legalCardActions.end());
            return legalGuessActions;
        }


        bool DealCard(int cardIndex) {
            assert(this->turn == kChancePlayerId && this->gameState == kDealing);
            if (this->cardsDealt < this->numPlayers * this->roundNr) {
                // deal cards
                auto const card = this->deck.DealCard(cardIndex);
                this->hands[dealTo].emplace_back(card);
                this->cardsDealt++;
                dealTo++;
                if (dealTo >= this->numPlayers) {
                    dealTo = 0;
                }
                return false;
            } else {
                // deal trump, if dealt all cards in the final round, set trump to noob (no trump)
                if (this->cardsDealt == kDeckSize) {
                    assert(cardIndex == 0);
                    this->trump = Card(cardIndex);
                } else {
                    this->trump = this->deck.DealCard(cardIndex);
                }
                this->gameState = kGuessing;
                this->turn = startPlayer;
                this->stopTurn = this->GetStopTurn();
                return true;
            }
        }

        bool GuessTricks(int n) {
            assert(this->gameState == kGuessing);
            auto const legalActions = GetLegalActions();
            if (std::find(legalActions.begin(), legalActions.end(), n) == legalActions.end()) {
                throw std::invalid_argument(absl::StrFormat("Guess %d not a legal action", n));
            }
            if (this->turn == this->stopTurn) {
                this->guessedTricks[this->turn] = n;
                this->turn = this->startPlayer;
                this->gameState = kTricking;
                return true;
            }
            this->guessedTricks[this->turn] = n;
            if (this->turn >= this->numPlayers - 1) {
                this->turn = 0;
            } else {
                this->turn += 1;
            }
            return false;
        }

        bool PlayCard(int actionId) {
            assert(this->gameState == kTricking);
            auto const &playersHand = this->hands[this->turn];
            // translate action to a card
            auto const card = Card(actionId - numGuessActions);
            // find this card in the players hand
            auto const it = std::find(playersHand.begin(), playersHand.end(), card);
            if (it == playersHand.end()) {
                throw std::invalid_argument(
                        absl::StrFormat("Card %s cannot be played by player %d", card.ToStr(), this->turn));
            }
//                assert(it != playersHand.end());
            auto const idx = std::distance(playersHand.begin(), it);
            this->hands[this->turn].erase(playersHand.begin() + idx);
            this->cardsPlayedOnTable.emplace_back(card);
            this->playedByOnTable.emplace_back(this->turn);
            // finished for this trick
            if (this->turn == this->stopTurn) {
                this->numTricks += 1;
                // finished for the whole round, set final to true
                if (this->numTricks >= this->roundNr) {
                    this->final = true;
                }
                return true;
            }

            if (this->turn >= this->numPlayers - 1) {
                this->turn = 0;
            } else {
                this->turn += 1;
            }
            return false;
        }

        void UpdateTricks() {
            assert(this->cardsPlayedOnTable.size() == this->numPlayers);
            auto card = this->cardsPlayedOnTable[0];
            int trickIndex = this->playedByOnTable[0];
            for (int i = 1; i < this->cardsPlayedOnTable.size(); i++) {
                auto nextCard = this->cardsPlayedOnTable[i];
                auto trick = card.Compare(nextCard, (Colors) this->trump.getColor());
                if (trick < 0) {
                    trickIndex = this->playedByOnTable[i];
                    card = nextCard;
                }
            }
            this->tricks[trickIndex] += 1;
            this->turn = trickIndex;
            this->cardsPlayed.insert(this->cardsPlayed.end(), this->cardsPlayedOnTable.begin(),
                                     this->cardsPlayedOnTable.end());
            this->cardsPlayedOnTable.clear();
            this->playedBy.insert(this->playedBy.end(), this->playedByOnTable.begin(),
                                  this->playedByOnTable.end());
            this->playedByOnTable.clear();
            this->stopTurn = this->GetStopTurn();
        }

        [[nodiscard]] std::vector<double> GetRewards(RewardMode mode) const {
            std::vector<double> rewards(this->numPlayers, 0);
            if (!this->final) {
                return rewards;
            }
            for (int i = 0; i < this->tricks.size(); i++) {
                auto const diff = abs(this->tricks[i] - this->guessedTricks[i]);
                if (diff == 0) {
                    rewards[i] = 20 + this->tricks[i] * 10;
                } else {
                    rewards[i] = diff * -10;
                }
            }
            if (mode == kBinaryReward) {
                for (auto &r: rewards) {
                    if (r > 0) {
                        r = 1;
                    } else {
                        r = -1;
                    }
                }
            }
            return rewards;
        }

        std::vector<std::vector<Card>> hands;

        [[nodiscard]] int getNumActions() const {
            return this->numActions;
        }

        [[nodiscard]] int getNumPlayers() const {
            return this->numPlayers;
        }

        [[nodiscard]] RewardMode getRewardMode() const {
            return this->rewardMode;
        }

        [[nodiscard]] int getMaxGameLength() const {
            return this->numPlayers * this->roundNr + this->roundNr;
        }

        [[nodiscard]] int getRoundNr() const {
            return this->roundNr;
        }

        [[nodiscard]] int getTurn() const {
            return this->turn;
        }

        [[nodiscard]] int IsFinal() const {
            return this->final;
        }

        [[nodiscard]] const std::vector<Card> &getCardsPlayed() const {
            return cardsPlayed;
        }

        [[nodiscard]] const std::vector<int> &getPlayedBy() const {
            return playedBy;
        }

        [[nodiscard]] const std::vector<Card> &getCardsPlayedOnTable() const {
            return cardsPlayedOnTable;
        }

        [[nodiscard]] const std::vector<int> &getPlayedByOnTable() const {
            return playedByOnTable;
        }

        [[nodiscard]] const std::vector<int> &getGuessedTricks() const {
            return guessedTricks;
        }

        [[nodiscard]] const std::vector<int> &getTricks() const {
            return tricks;
        }

        [[nodiscard]] const Deck &getDeck() const {
            return deck;
        }

        [[nodiscard]] Card getTrump() const {
            return trump;
        }

        [[nodiscard]] int getStartPlayer() const {
            return startPlayer;
        }

        [[nodiscard]] GameState getGameState() const {
            return gameState;
        }

        [[nodiscard]] int getNumGuessActions() const {
            return numGuessActions;
        }

        [[nodiscard]] int getCardsDealt() const {
            return cardsDealt;
        }

    private:
        std::vector<Card> cardsPlayed;
        std::vector<int> playedBy;
        std::vector<Card> cardsPlayedOnTable{};
        std::vector<int> playedByOnTable;
        GameState gameState{};
        int numPlayers{};
        int startPlayer{};
        int turn{};
        int dealTo{};
        int stopTurn{};
        int roundNr{};
        std::vector<int> guessedTricks;
        std::vector<int> tricks;
        Deck deck;
        int cardsDealt{};
        Card trump{};
        int numGuessActions{};
        bool final{};
        int numTricks{};
        RewardMode rewardMode = kNormalReward;
        int numActions{};

        [[nodiscard]] std::vector<int> GetLegalCardActions() const {
            return this->GetLegalCardActions(this->turn);
        }

        [[nodiscard]] std::vector<int> GetLegalCardActions(int playerNr) const {
            // cant take card actions during guessing stage or if im not on turn
            if (this->gameState == kGuessing || this->turn != playerNr) {
                return {};
            }
            Card firstRelevantCard(kWhite, kNoobValue);
            for (auto const &card: this->cardsPlayedOnTable) {
                if (!card.isNoob()) {
                    firstRelevantCard = card;
                    break;
                }
            }
            std::vector<int> distinctLegalActions;
            std::set<int> seenActions;
            if (!firstRelevantCard.isNoob() && !firstRelevantCard.isWizard()) {
                bool gotSameColorInHand(false);
                for (auto const &card : this->hands[playerNr]) {
                    if (card.getColor() == firstRelevantCard.getColor()) {
                        gotSameColorInHand = true;
                        break;
                    }
                }
                if (gotSameColorInHand) {
                    for (auto card : this->hands[playerNr]) {
                        if (card.getColor() == firstRelevantCard.getColor() || card.getColor() == kWhite) {
                            auto const idx = card.ToIdx() + numGuessActions; // important since first
                            // numGuessActions actions are reserved for the guess actions
//                                distinctLegalActions.insert(idx);
                            if (seenActions.find(idx) != seenActions.end()) {
                                continue;
                            }
                            distinctLegalActions.emplace_back(idx);
                            seenActions.emplace(idx);
                        }
                    }
                    std::sort(distinctLegalActions.begin(), distinctLegalActions.end());
                    return distinctLegalActions;
                }
            }
            // if we reached this, we can play all cards in our hand
            for (auto card : this->hands[playerNr]) {
                auto const idx = card.ToIdx() + numGuessActions;
                if (seenActions.find(idx) != seenActions.end()) {
                    continue;
                }
                distinctLegalActions.emplace_back(idx);
                seenActions.emplace(idx);
            }
            std::sort(distinctLegalActions.begin(), distinctLegalActions.end());
            return distinctLegalActions;
        }

        [[nodiscard]] std::vector<int> GetLegalGuessActions() const {
            return this->GetLegalGuessActions(this->turn);
        }

        [[nodiscard]] std::vector<int> GetLegalGuessActions(int playerNr) const {
            std::vector<int> legalActions = {};
            // cant take guess actions during tricking stage or if im not on turn
            if (this->gameState == kTricking || this->turn != playerNr) {
                return legalActions;
            }
            int sumGuessedTricks = 0;
            for (auto &n : this->guessedTricks) {
                sumGuessedTricks += n;
            }
            for (int i = 0; i <= this->roundNr; i++) {
                if (sumGuessedTricks + i == this->roundNr && playerNr == this->stopTurn) {
                    continue;
                }
                legalActions.emplace_back(i);
            }
            return legalActions;
        }

        [[nodiscard]] int GetStopTurn() const {
            int stopAt = this->turn - 1;
            if (stopAt < 0) {
                stopAt = this->numPlayers - 1;
            }
            return stopAt;
        }
    };

    class WizardObserver;

    class WizardState : public State {
    public:
        ~WizardState() override = default;

        WizardState(const std::shared_ptr<const Game> &game, RewardMode rewardMode, int startPlayer, int roundNr);

        WizardState(const WizardState &) = default;

        [[nodiscard]] Player CurrentPlayer() const override;

        [[nodiscard]] std::vector<Action> LegalActions() const override;

        [[nodiscard]] std::string ActionToString(Player player, Action action_id) const override;

        [[nodiscard]] std::string ToString() const override;

        [[nodiscard]] bool IsTerminal() const override;

        [[nodiscard]] std::vector<double> Returns() const override;

        [[nodiscard]] ActionsAndProbs ChanceOutcomes() const override;

        [[nodiscard]] std::string InformationStateString(Player player) const override;

        std::unique_ptr<State> ResampleFromInfostate(int player_id, std::function<double()> rng) const override;

        void InformationStateTensor(Player player, absl::Span<float> values) const override;

        void ObservationTensor(Player player, absl::Span<float> values) const override;

        std::string ObservationString(Player player) const override;

        [[nodiscard]] std::unique_ptr<State> Clone() const override;

    protected:
        void DoApplyAction(Action action_id) override;

    private:
        friend class WizardObserver;

        Round r;
        RewardMode reward_mode_;
        std::vector<int> history_by_;
    };

    class WizardGame : public Game {
    public:
        ~WizardGame() override = default;

        explicit WizardGame(const GameParameters &params);

        int NumDistinctActions() const override;

        std::unique_ptr<State> NewInitialState() const override;

        int NumPlayers() const override;

        double MinUtility() const override;

        double MaxUtility() const override;

        int MaxChanceOutcomes() const override;

        std::shared_ptr<const Game> Clone() const override;

        std::vector<int> InformationStateTensorShape() const override;

        std::vector<int> ObservationTensorShape() const override;

        int MaxGameLength() const override;

        std::shared_ptr<Observer>
        MakeObserver(absl::optional<IIGObservationType> iig_obs_type, const GameParameters &params) const override;

        std::shared_ptr<WizardObserver> infoStateObserver_;
        std::shared_ptr<WizardObserver> defaultObserver_;

    private:
        int num_players_;
        RewardMode reward_mode_;
        int start_player_;
        int round_nr_;
    };

}  // namespace open_spiel

#endif  // THIRD_PARTY_OPEN_SPIEL_GAMES_WIZARD_H_
