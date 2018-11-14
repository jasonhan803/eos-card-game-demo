#include "gameplay.cpp"

void cardgame::login(account_name username) {
  // Ensure this action is authorized by the player
  require_auth(username);
  
  // Create a record in the table if the player doesn't exist in our app yet
  auto user_iterator = _users.find(username);
  if (user_iterator == _users.end()) {
    user_iterator = _users.emplace(username,  [&](auto& new_user) {
      new_user.name = username;
    });
  } 
}

void cardgame::startgame(account_name username) {
  // Ensure this action is authorized by the player
  require_auth(username);

  auto& user = _users.get(username, "User doesn't exist");
  
  _users.modify(user, username, [&](auto& modified_user) {
    // Create a new game
    game game_data;

    // Draw 4 cards each for the player and the AI
    for (uint8_t i = 0; i < 4; i++) {
      draw_one_card(game_data.deck_player, game_data.hand_player);
      draw_one_card(game_data.deck_ai, game_data.hand_ai);
    }

    // Assign the newly created game to the player
    modified_user.game_data = game_data;
  });
}

void cardgame::endgame(account_name username){

  require_auth(username);

  // Get the user and reset the game
  auto& user = _users.get(username, "User doesn't exist");
  _users.modify(user, username, [&](auto& modified_user) {
  modified_user.game_data = game();
  });
}
void cardgame::playcard(account_name username, uint8_t player_card_idx) {
  // Ensure this action is authorized by the player
  require_auth(username);

  // Checks that selected card is valid
  eosio_assert(player_card_idx < 4, "playcard: Invalid hand index");

  auto& user = _users.get(username, "User doesn't exist");

  // Verify game status is suitable for the player to play a card
  eosio_assert(user.game_data.status == ONGOING,
            "playcard: This game has ended. Please start a new one");
  eosio_assert(user.game_data.selected_card_player == 0,
            "playcard: The player has played his card this turn!");

    _users.modify(user, username, [&](auto& modified_user) {
    game& game_data = modified_user.game_data;

    // Assign the selected card from the player's hand
    game_data.selected_card_player = game_data.hand_player[player_card_idx];
    game_data.hand_player[player_card_idx] = 0;


    // AI picks a card
    int ai_card_idx = ai_choose_card(game_data);
    game_data.selected_card_ai = game_data.hand_ai[ai_card_idx];
    game_data.hand_ai[ai_card_idx] = 0;

    resolve_selected_cards(game_data)

    update_game_status(modified_user);

  });
}

// Resolve selected cards and update the damage dealt
void cardgame::resolve_selected_cards(game& game_data) {
  const auto player_card = card_dict.at(game_data.selected_card_player);
  const auto ai_card = card_dict.at(game_data.selected_card_ai);

  // For type VOID, we will skip any damage calculation
  if (player_card.type == VOID || ai_card.type == VOID) return;

  int player_attack_point = calculate_attack_point(player_card, ai_card);
  int ai_attack_point =  calculate_attack_point(ai_card, player_card);

    // Damage calculation
  if (player_attack_point > ai_attack_point) {
    // Deal damage to the AI if the AI card's attack point is higher
    int diff = player_attack_point - ai_attack_point;
    game_data.life_lost_ai = diff;
    game_data.life_ai -= diff;
  } else if (ai_attack_point > player_attack_point) {
    // Deal damage to the player if the player card's attack point is higher
    int diff = ai_attack_point - player_attack_point;
    game_data.life_lost_player = diff;
    game_data.life_player -= diff;
  }
}

void cardgame::nextround(account_name username) {
  // Ensure this action is authorized by the player
  require_auth(username);

  auto& user = _users.get(username, "User doesn't exist");

  // Verify game status
  eosio_assert(user.game_data.status == ONGOING, 
              "nextround: This game has ended. Please start a new one.");
  eosio_assert(user.game_data.selected_card_player != 0 && user.game_data.selected_card_ai != 0,
              "nextround: Please play a card first.");

  _users.modify(user, username, [&](auto& modified_user) {
    game& game_data = modified_user.game_data;

    // Reset selected card and damage dealt
    game_data.selected_card_player = 0;
    game_data.selected_card_ai = 0;
    game_data.life_lost_player = 0;
    game_data.life_lost_ai = 0;


    // Draw card for the player and the AI
    if (game_data.deck_player.size() > 0) draw_one_card(game_data.deck_player, game_data.hand_player);
    if (game_data.deck_ai.size() > 0) draw_one_card(game_data.deck_ai, game_data.hand_ai);
  });

}


EOSIO_ABI(cardgame, (login)(startgame)(playcard)(nextround)(endgame))