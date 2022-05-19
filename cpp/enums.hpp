enum class Direction {
    Up,
    Right,
    Down,
    Left
};

enum class Event {
    BombPlaced,
    BombExploded,
    PlayerMoved,
    BlockPlaced
};

enum class ClientMessage {
    Join,
    PlaceBomb,
    PlaceBlock,
    Move
};

enum class ServerMessage {
    Hello,
    AcceptedPlayer,
    GameStarted,
    Turn,
    GameEnded
};

enum class InputMessage {
    PlaceBomb,
    PlaceBlock,
    Move
};

enum class DrawMessage {
    Lobby,
    Game
};