#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

// アプリケーション全体で共有する列挙型や構造体を定義します

enum SortMode {
    SortName,       // 名前順 (自然順)
    SortDate,       // 日付順
    SortSize,       // サイズ順
    SortShuffle     // シャッフル
};

#endif // COMMON_TYPES_H
