#include <Arduino.h>
#include <Wire.h>
#include <ShiftRegister74HC595.h>

// ------------------------------------------------------------
// 基本設定
// ------------------------------------------------------------
#define I2C_ADDRESS 0x2A
#define DIN 4                 // データ信号出力ピン
#define LE  0                 // ラッチ動作出力ピン
#define CLK 1                 // クロック信号出力ピン

// 5段シフトレジスタ
ShiftRegister74HC595<5> sr(DIN, CLK, LE);
uint8_t pinValues[5] = {0, 0, 0, 0, 0};

// ------------------------------------------------------------
// 基本制御定数・変数
// ------------------------------------------------------------
const uint8_t PWM_STEPS = 20;           // PWM解像度
volatile uint8_t globalPwmCounter = 0;   // ISR用PWMカウンター
volatile uint8_t currentDigit = 0;       // ISR用現在表示桁
volatile uint32_t mainLoopCounter = 0;   // メインループ用タイマー

// ------------------------------------------------------------
// テーブル構成（メモリ効率重視）
// ------------------------------------------------------------

// 1. ISR用表示テーブル（ISRはこのテーブルのみ参照）
volatile uint8_t displayTable[8] = {0, 1, 2, 3, 4, 5, 6, 7};      // 表示数字
volatile uint8_t brightnessTable[8] = {20, 20, 20, 20, 20, 20, 20, 20}; // 表示輝度

// 1a. ISR用ドット表示テーブル
volatile uint8_t dotLTable[8] = {0, 0, 0, 0, 0, 0, 0, 0};         // 左ドット表示状態
volatile uint8_t dotRTable[8] = {0, 0, 0, 0, 0, 0, 0, 0};         // 右ドット表示状態

// 2. 管別輝度補正テーブル（削除してメモリ節約）
// uint8_t brightnessCorrection[8] = {
//   100, 100, 100, 85, 100, 100, 100, 85  // 管3,7は15%減光
// };

// 3. 新数字テーブル（I2C受信後の目標値）
uint8_t newNumberTable[8] = {0, 1, 2, 3, 4, 5, 6, 7};

// 4. 古数字テーブル（エフェクト用の前回値）
uint8_t oldNumberTable[8] = {0, 1, 2, 3, 4, 5, 6, 7};

// 5. エフェクト制御テーブル
uint8_t effectType[8] = {0, 0, 0, 0, 0, 0, 0, 0};        // 0=なし, 1=クロスフェード, 2=シャッフル
uint8_t effectStep[8] = {0, 0, 0, 0, 0, 0, 0, 0};        // エフェクト進行ステップ
uint8_t effectDuration[8] = {5, 5, 5, 5, 5, 5, 5, 5};    // エフェクト時間（デフォルト5ステップ=約1秒）

// 6. クロスフェード制御
uint32_t lastEffectTime = 0;  // エフェクトタイミング制御

// 7. シャッフル制御
uint32_t lastShuffleTime = 0;     // シャッフルタイミング制御
uint8_t shuffleSpeed = 26;        // シャッフル速度（デフォルト約20ms間隔）
uint16_t shuffleSeed = 12345;     // 擬似乱数シード

// 8. シャッフル停止時の目標数字テーブル
uint8_t shuffleTargetTable[8] = {0, 1, 2, 3, 4, 5, 6, 7};

// クロスフェード速度テーブル
const uint8_t CROSSFADE_SPEED_TABLE[7] PROGMEM = {
  13, 26, 33, 66, 132, 198, 264
};

uint8_t crossfadeSpeedIndex = 3;  // デフォルト：標準

// 9. I2C制御
uint8_t receiveBuffer[8];
volatile bool i2cCommandFlag = false;
bool lockFLG = false;

// ------------------------------------------------------------
// ISR - PWM制御とダイナミック表示（特化）
// ------------------------------------------------------------
ISR(TCA0_OVF_vect) {
  // カウンター更新
  mainLoopCounter++;
  globalPwmCounter++;
  if (globalPwmCounter >= PWM_STEPS) {
    globalPwmCounter = 0;
  }
  
  // pinValues初期化
  for (uint8_t i = 0; i < 5; i++) {
    pinValues[i] = 0;
  }
  
  // 現在桁の数字と輝度を取得
  uint8_t number = displayTable[currentDigit];
  uint8_t brightness = brightnessTable[currentDigit];
  
  // クロスフェード中は高速時分割処理
  if (effectType[currentDigit] == 1) { // クロスフェード中
    // 超高速時分割（約1msサイクル）でチカチカ感を完全除去
    // 各桁で位相をずらしてパターンを分散
    uint8_t fastCycle = (mainLoopCounter + currentDigit * 3) % 20; // 位相分散
    uint8_t oldRatio = effectDuration[currentDigit] - effectStep[currentDigit];
    uint8_t newRatio = effectStep[currentDigit] + 1;
    
    // 段階に応じた表示時間比率（古い数字の表示割合を計算）
    uint8_t oldThreshold = (oldRatio * 20) / effectDuration[currentDigit];
    
    if (fastCycle < oldThreshold) {
      // 古い数字を表示（段階的に表示時間を短く）
      number = oldNumberTable[currentDigit];
    } else {
      // 新しい数字を表示（段階的に表示時間を長く）
      number = newNumberTable[currentDigit];
    }
    
    // フル輝度で表示（時分割により視覚的に輝度調整される）
    brightness = PWM_STEPS;
    
    // 輝度補正適用（簡略化）
    if (currentDigit < 8) {
      // brightness = (brightness * brightnessCorrection[currentDigit]) / 100;
      if (brightness > PWM_STEPS) brightness = PWM_STEPS;
    }
  }
  
  // PWM制御で表示判定
  if (globalPwmCounter < brightness && number <= 9) {
    // カソード設定（数字0-9）- 修正版
    if (number <= 7) {
      // 数字0-7: pinValues[0]に設定
      pinValues[0] |= (1 << number);
    } else if (number == 8) {
      // 数字8: pinValues[1]のビット0に設定
      pinValues[1] |= (1 << 0);
    } else if (number == 9) {
      // 数字9: pinValues[1]のビット1に設定
      pinValues[1] |= (1 << 1);
    }
    
    // アノード設定（管0-7）
    pinValues[4] |= (1 << currentDigit);
  }
  
  // ドット表示処理（数字の状態に関係なく独立して動作）
  if (dotLTable[currentDigit]) {
    pinValues[1] |= (1 << 2);  // 左ドット
    pinValues[4] |= (1 << currentDigit);  // ドット用アノード
  }
  if (dotRTable[currentDigit]) {
    pinValues[1] |= (1 << 3);  // 右ドット
    pinValues[4] |= (1 << currentDigit);  // ドット用アノード
  }
  
  // シフトレジスタ出力
  sr.setAll(pinValues);
  
  // 次の桁へ
  currentDigit++;
  if (currentDigit >= 8) {
    currentDigit = 0;
  }
  
  // 割り込みフラグクリア
  TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
}

// ------------------------------------------------------------
// 基本制御関数
// ------------------------------------------------------------

// 簡単な擬似乱数生成器（線形合同法）
uint16_t pseudoRandom() {
  shuffleSeed = (shuffleSeed * 1103515245 + 12345) & 0x7FFFFFFF;
  return shuffleSeed;
}

// 0-9の乱数を生成
uint8_t getRandomDigit() {
  return pseudoRandom() % 10;
}

// 輝度補正を適用した輝度計算（簡略版）
uint8_t calculateBrightness(uint8_t tube, uint8_t basePercent) {
  if (tube >= 8) return 0;
  uint8_t brightness = (PWM_STEPS * basePercent) / 100;
  return (brightness > PWM_STEPS) ? PWM_STEPS : brightness;
}

// 消灯機能（メモリ最適化版）
void setTubeState(uint8_t tube, uint8_t digitState, uint8_t dotState) {
  if (tube >= 8) return;
  noInterrupts();
  if (digitState == 255) {
    displayTable[tube] = 255;  // 数字消灯
    brightnessTable[tube] = 0;
  }
  if (dotState == 0) {
    dotLTable[tube] = 0;
    dotRTable[tube] = 0;
  }
  interrupts();
}

// 表示テーブル更新（ISR用テーブルを安全に更新）
void updateDisplayTable() {
  noInterrupts();
  for (uint8_t i = 0; i < 8; i++) {
    displayTable[i] = newNumberTable[i];
    brightnessTable[i] = calculateBrightness(i, 100);  // デフォルト100%
  }
  interrupts();
}

// ------------------------------------------------------------
// クロスフェード制御関数
// ------------------------------------------------------------

// クロスフェード開始
void startCrossfade(uint8_t tube, uint8_t newNumber) {
  if (tube >= 8 || newNumber > 9) return;
  // 消灯状態からの復帰や同じ数字でも強制実行
  
  effectType[tube] = 1;  // クロスフェード有効
  effectStep[tube] = 0;  // ステップリセット
  newNumberTable[tube] = newNumber;  // 新しい数字設定
}

// ------------------------------------------------------------
// シャッフル制御関数
// ------------------------------------------------------------

// シャッフル開始（全管）
void startShuffleAll() {
  for (uint8_t i = 0; i < 8; i++) {
    effectType[i] = 2;  // シャッフル有効
    effectStep[i] = 0;  // ステップリセット
  }
}

// シャッフル開始（個別管）
void startShuffle(uint8_t tube) {
  if (tube >= 8) return;
  effectType[tube] = 2;  // シャッフル有効
  effectStep[tube] = 0;  // ステップリセット
}

// シャッフル停止（全管）
void stopShuffleAll() {
  for (uint8_t i = 0; i < 8; i++) {
    if (effectType[i] == 2) {  // シャッフル中の管のみ
      // 停止処理（強制的にシャッフル終了）
      effectType[i] = 0;  // まず一旦シャッフルを停止
      // 新→古コピー（重要！）
      oldNumberTable[i] = displayTable[i];
      // クロスフェードで目標数字に移行（同じ数字でも強制実行）
      if (oldNumberTable[i] != shuffleTargetTable[i]) {
        startCrossfade(i, shuffleTargetTable[i]);
      } else {
        // 同じ数字の場合は直接設定
        noInterrupts();
        displayTable[i] = shuffleTargetTable[i];
        brightnessTable[i] = calculateBrightness(i, 100);
        interrupts();
      }
    }
  }
}

// シャッフル停止（個別管、停止時表示数字指定）
void stopShuffle(uint8_t tube, uint8_t targetNumber) {
  if (tube >= 8 || targetNumber > 9) return;
  if (effectType[tube] == 2) {  // シャッフル中の管のみ
    // 停止処理（強制的にシャッフル終了）
    effectType[tube] = 0;  // まず一旦シャッフルを停止
    // 停止時の目標数字を設定
    shuffleTargetTable[tube] = targetNumber;
    // 新→古コピー（重要！）
    oldNumberTable[tube] = displayTable[tube];
    // クロスフェードで目標数字に移行（同じ数字でも強制実行）
    if (oldNumberTable[tube] != targetNumber) {
      startCrossfade(tube, targetNumber);
    } else {
      // 同じ数字の場合は直接設定
      noInterrupts();
      displayTable[tube] = targetNumber;
      brightnessTable[tube] = calculateBrightness(tube, 100);
      interrupts();
    }
  }
}

// シャッフル処理（loop内で実行）
void processShuffles() {
  // タイミング制御
  if (mainLoopCounter - lastShuffleTime < shuffleSpeed) return;
  lastShuffleTime = mainLoopCounter;
  
  // 各管のシャッフル処理
  for (uint8_t i = 0; i < 8; i++) {
    if (effectType[i] == 2) { // シャッフル中
      // ランダム数字を表示テーブルに直接設定
      uint8_t randomDigit = getRandomDigit();
      noInterrupts();
      displayTable[i] = randomDigit;
      brightnessTable[i] = calculateBrightness(i, 100);
      interrupts();
    }
  }
}

// エフェクト処理（loop内で実行）
void processEffects() {
  // タイミング制御（PROGMEMから読み取り）
  uint8_t speed = pgm_read_byte(&CROSSFADE_SPEED_TABLE[crossfadeSpeedIndex]);
  if (mainLoopCounter - lastEffectTime < speed) return;
  lastEffectTime = mainLoopCounter;
  
  // 各管のエフェクト処理
  for (uint8_t i = 0; i < 8; i++) {
    if (effectType[i] == 1) { // クロスフェード中
      effectStep[i]++;
      
      if (effectStep[i] >= effectDuration[i]) {
        // クロスフェード完了
        effectType[i] = 0;  // エフェクト終了
        effectStep[i] = 0;
        
        // 表示テーブル更新（新しい数字で確定）
        noInterrupts();
        displayTable[i] = newNumberTable[i];
        brightnessTable[i] = calculateBrightness(i, 100);
        interrupts();
      }
    }
  }
}

// I2C受信処理
void receiveEvent(int howMany) {
  uint8_t i = 0;
  while (Wire.available() && i < 8) {
    receiveBuffer[i] = Wire.read();
    i++;
  }
  i2cCommandFlag = true;
}

// I2C要求処理
void requestEvent() {
  // 必要に応じて状態送信
}

// I2Cコマンド処理（基本版）
void processI2CCommand() {
  if (!i2cCommandFlag) return;
  
  noInterrupts();
  i2cCommandFlag = false;
  interrupts();
  
  if (lockFLG && receiveBuffer[0] != 0x4D) return;
  
  uint8_t cmd = receiveBuffer[0];
  
  // 基本コマンド処理
  if (cmd <= 0x09) {
    // 全管同じ数字（0x00-0x09）
    for (uint8_t i = 0; i < 8; i++) {
      // シャッフル中の場合は停止
      if (effectType[i] == 2) {
        stopShuffle(i, cmd);
      } else {
        // 消灯状態からの復帰も含めて処理
        // 新→古コピー（重要！）
        oldNumberTable[i] = (displayTable[i] == 255) ? cmd : newNumberTable[i];
        // 直接設定（消灯状態からの復帰）
        noInterrupts();
        displayTable[i] = cmd;
        newNumberTable[i] = cmd;
        brightnessTable[i] = calculateBrightness(i, 100);
        interrupts();
        // クロスフェード開始（同じ数字でも復帰処理）
        if (oldNumberTable[i] != cmd) {
          startCrossfade(i, cmd);
        }
      }
    }
  }
  else if (cmd >= 0x10 && cmd <= 0x17) {
    // 管別数字設定（0x10-0x17）
    uint8_t tube = cmd - 0x10;
    uint8_t number = receiveBuffer[1];
    
    if (number <= 9) {
      // シャッフル中の場合は停止
      if (effectType[tube] == 2) {
        stopShuffle(tube, number);
      } else {
        // 消灯状態からの復帰も含めて処理
        // 新→古コピー
        oldNumberTable[tube] = (displayTable[tube] == 255) ? number : newNumberTable[tube];
        // 直接設定（消灯状態からの復帰）
        noInterrupts();
        displayTable[tube] = number;
        newNumberTable[tube] = number;
        brightnessTable[tube] = calculateBrightness(tube, 100);
        interrupts();
        // クロスフェード開始（同じ数字でも復帰処理）
        if (oldNumberTable[tube] != number) {
          startCrossfade(tube, number);
        }
      }
    }
  }
  else if (cmd >= 0x20 && cmd <= 0x27) {
    // 管別左ドット点灯（0x20-0x27）
    uint8_t tube = cmd - 0x20;
    uint8_t state = receiveBuffer[1];
    if (tube < 8) {
      noInterrupts();
      dotLTable[tube] = (state > 0) ? 1 : 0;
      interrupts();
    }
  }
  else if (cmd >= 0x30 && cmd <= 0x37) {
    // 管別右ドット点灯（0x30-0x37）
    uint8_t tube = cmd - 0x30;
    uint8_t state = receiveBuffer[1];
    if (tube < 8) {
      noInterrupts();
      dotRTable[tube] = (state > 0) ? 1 : 0;
      interrupts();
    }
  }
  else if (cmd == 0x60) {
    // クロスフェード制御（0x60）
    uint8_t param = receiveBuffer[1];
    if (param >= 1 && param <= 10) {  // 1-10ステップに制限
      // 全管のクロスフェード時間設定
      for (uint8_t i = 0; i < 8; i++) {
        effectDuration[i] = param;
      }
    }
  }
  else if (cmd >= 0x61 && cmd <= 0x68) {
    // 管別クロスフェード時間設定（0x61-0x68）
    uint8_t tube = cmd - 0x61;
    uint8_t duration = receiveBuffer[1];
    if (tube < 8 && duration >= 1 && duration <= 10) {  // 1-10ステップに制限
      effectDuration[tube] = duration;
    }
  }
  else if (cmd == 0x70) {
    // クロスフェード速度設定（0x70）
    uint8_t speed = receiveBuffer[1];
    if (speed <= 6) {  // 0-6の範囲に制限
      crossfadeSpeedIndex = speed;
    }
  }
  else if (cmd == 0x71) {
    // シャッフル速度設定（0x71）
    uint8_t speed = receiveBuffer[1];
    if (speed >= 10 && speed <= 200) {  // 10-200の範囲に制限（約8-150ms間隔）
      shuffleSpeed = speed;
    }
  }
  else if (cmd == 0x72) {
    // 全管シャッフル開始（0x72）
    startShuffleAll();
  }
  else if (cmd == 0x73) {
    // 全管シャッフル停止（0x73）
    // パラメータで停止時表示数字を指定
    uint8_t stopNumber = receiveBuffer[1];
    if (stopNumber <= 9) {
      for (uint8_t i = 0; i < 8; i++) {
        shuffleTargetTable[i] = stopNumber;
      }
      stopShuffleAll();
    }
  }
  else if (cmd >= 0x74 && cmd <= 0x7B) {
    // 管別シャッフル開始（0x74-0x7B）
    uint8_t tube = cmd - 0x74;
    if (tube < 8) {
      startShuffle(tube);
    }
  }
  else if (cmd >= 0x7C && cmd <= 0x7F) {
    // 予約領域（将来拡張用）
    if (cmd == 0x7C) {
      // 緊急シャッフル停止（0x7C）- 全管のシャッフルを強制停止
      for (uint8_t i = 0; i < 8; i++) {
        if (effectType[i] == 2) {  // シャッフル中の管のみ
          effectType[i] = 0;  // シャッフルを強制停止
          // 現在の表示数字で固定
          noInterrupts();
          brightnessTable[i] = calculateBrightness(i, 100);
          interrupts();
        }
      }
    }
  }
  else if (cmd == 0x80) {
    // 全管左ドット点灯（0x80）
    uint8_t state = receiveBuffer[1];
    noInterrupts();
    for (uint8_t i = 0; i < 8; i++) {
      dotLTable[i] = (state > 0) ? 1 : 0;
    }
    interrupts();
  }
  else if (cmd == 0x81) {
    // 全管右ドット点灯（0x81）
    uint8_t state = receiveBuffer[1];
    noInterrupts();
    for (uint8_t i = 0; i < 8; i++) {
      dotRTable[i] = (state > 0) ? 1 : 0;
    }
    interrupts();
  }
  else if (cmd >= 0x90 && cmd <= 0xA7) {
    // 管別シャッフル停止＋数字指定（0xA0-0xA7のみ有効）
    if (cmd >= 0xA0 && cmd <= 0xA7) {
      uint8_t tube = cmd - 0xA0;
      uint8_t targetNumber = receiveBuffer[1];
      if (tube < 8 && targetNumber <= 9) {
        stopShuffle(tube, targetNumber);
      }
    }
  }
  else if (cmd == 0xBF) {
    // 全管左ドット消灯（0xBF）
    noInterrupts();
    for (uint8_t i = 0; i < 8; i++) {
      dotLTable[i] = 0;
    }
    interrupts();
  }
  else if (cmd == 0xCF) {
    // 全管右ドット消灯（0xCF）
    noInterrupts();
    for (uint8_t i = 0; i < 8; i++) {
      dotRTable[i] = 0;
    }
    interrupts();
  }
  else if (cmd == 0xDF) {
    // 全管ドット消灯（LR）（0xDF）
    noInterrupts();
    for (uint8_t i = 0; i < 8; i++) {
      dotLTable[i] = 0;
      dotRTable[i] = 0;
    }
    interrupts();
  }
  else if (cmd >= 0xD1 && cmd <= 0xD7) {
    // 管別ドット消灯（LR）（0xD1-0xD7）
    uint8_t tube = cmd - 0xD1;
    if (tube < 8) {
      noInterrupts();
      dotLTable[tube] = 0;
      dotRTable[tube] = 0;
      interrupts();
    }
  }
  else if (cmd >= 0xE0 && cmd <= 0xE7) {
    // 管別数字消灯（0xE0-0xE7）
    uint8_t tube = cmd - 0xE0;
    setTubeState(tube, 255, 1);
  }
  else if (cmd == 0xEF) {
    // 全管数字消灯（0xEF）
    noInterrupts();
    for (uint8_t i = 0; i < 8; i++) {
      displayTable[i] = 255;
      brightnessTable[i] = 0;
    }
    interrupts();
  }
  else if (cmd >= 0xF0 && cmd <= 0xF7) {
    // 管別全消灯（数字+LRドット）（0xF0-0xF7）
    uint8_t tube = cmd - 0xF0;
    setTubeState(tube, 255, 0);
  }
  else if (cmd == 0xFF) {
    // 全管全消灯（数字+LRドット）（0xFF）
    noInterrupts();
    for (uint8_t i = 0; i < 8; i++) {
      displayTable[i] = 255;
      brightnessTable[i] = 0;
      dotLTable[i] = 0;
      dotRTable[i] = 0;
    }
    interrupts();
  }
  else if (cmd == 0x4D) {
    // ロック解除 "MSX"
    if (receiveBuffer[1] == 0x53 && receiveBuffer[2] == 0x58) {
      lockFLG = false;
    }
  }
}

// ------------------------------------------------------------
// setup / loop
// ------------------------------------------------------------
void setup() {
  // I2C初期化
  Wire.begin(I2C_ADDRESS);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
  
  // シフトレジスタ初期化
  sr.setAllLow();
  
  // 初期数字テーブル設定（全管を対応する数字に設定）
  for (uint8_t i = 0; i < 8; i++) {
    newNumberTable[i] = i;        // 管0→数字0, 管1→数字1, ...
    oldNumberTable[i] = i;        // 初期状態では新旧同じ
    effectType[i] = 0;            // エフェクトなし
    effectStep[i] = 0;            // ステップ0
    effectDuration[i] = 5;        // デフォルト5ステップ
  }
  
  // タイマー初期化
  TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV64_gc;
  TCA0.SINGLE.PER = 1249;  // 1440Hz確認済み
  TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
  TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm;
  
  // 初期表示設定（ISR有効後に実行）
  updateDisplayTable();
}

void loop() {
  // I2Cコマンド処理
  processI2CCommand();
  
  // センサーロック中はスキップ
  if (lockFLG) return;

  // シャッフル処理
  processShuffles();
  
  // エフェクト処理
  processEffects();
}
