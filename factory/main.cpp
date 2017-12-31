#include "mbed.h"
#include "SakuraIO.h"

// プロトタイプ宣言
uint8_t monitor();
uint8_t dequeue();

DigitalOut status_led(PA_15);
DigitalOut patlite_r(PA_0); // R
DigitalOut patlite_y(PA_1); // Y
DigitalOut patlite_g(PA_2); // G
DigitalOut patlite_b(PA_3); // B
DigitalOut patlite_c(PA_4); // C
DigitalIn patlite_state_r(PA_12); // R
DigitalIn patlite_state_y(PA_8);  // Y
DigitalIn patlite_state_g(PA_7);  // G
DigitalIn patlite_state_b(PA_6);  // B
DigitalIn patlite_state_c(PA_5);  // C

DigitalOut patlite_out[5] = {patlite_g, patlite_r, patlite_y, patlite_b, patlite_c};
DigitalIn patlite_state[5] = {patlite_state_g, patlite_state_r, patlite_state_y, patlite_state_b, patlite_state_c};

I2C i2c(PB_7, PB_6); // sda, scl
SakuraIO_I2C sakuraio(i2c);

uint8_t last_state[5] = {0};

int main()
{
    // パトライトを1段ずつ順に点滅させながら接続待ち
    for (int patlite_index = 0;; patlite_index = (patlite_index + 1) % 5)
    {
        if ((sakuraio.getConnectionStatus() & 0x80) == 0x80)
            break;

        status_led = 1;
        patlite_out[patlite_index] = 1;
        wait_ms(500);
        status_led = 0;
        patlite_out[patlite_index] = 0;
        wait_ms(500);
    }

    // 接続完了したら全段1度点滅
    status_led = 1;
    for (int i = 0; i < 5; i++)
    {
        patlite_out[i] = 1;
    }
    wait(1);
    status_led = 0;
    for (int i = 0; i < 5; i++)
    {
        patlite_out[i] = 0;
    }

    while (1)
    {
        // ピン状態と新着メッセージを監視してどちらか変化があったら1
        uint8_t changed = monitor() | dequeue();
        if (changed == 1)
        {
            uint8_t state[5];
            for (int i = 0; i < 5; i++)
            {
                state[i] = patlite_state[i];
            }
            for (int i = 0; i < 5; i++)
            {
                sakuraio.enqueueTx(i, (int32_t)state[i]);
            }
            sakuraio.send();
        }
        wait_ms(10);
    }
}

// 外部からパトライトの点灯状態が変えられたことを検知する
uint8_t monitor()
{
    uint8_t changed = 0;
    uint8_t state[5];
    for (int i = 0; i < 5; i++)
    {
        // 負論理なのでNOTかけてから点灯状態として取り込み
        state[i] = !patlite_state[i];
    }

    for (int i = 0; i < 5; i++)
    {
        if (last_state[i] != state[i])
        {
            changed = 1;
            status_led = !status_led;
            last_state[i] = state[i];
        }
    }

    return changed;
}

// sakura.ioのRxキューの状態を見て操作命令が来ていないか確認する
uint8_t dequeue()
{
    uint8_t avail = 0, queued = 0, changed = 0;
    sakuraio.getRxQueueLength(&avail, &queued);
    for (uint8_t i = 0; i < queued; i++)
    {
        uint8_t channel, type, values[8];
        int64_t offset;

        // RXキューから点灯要求を取り出す
        uint8_t ret = sakuraio.dequeueRx(&channel, &type, values, &offset);
        if (ret == 0x01)
        {
            if (channel < 5)
            {
                if (*((int32_t *)values) == 0)
                    patlite_out[channel] = 0;
                else
                    patlite_out[channel] = 1;
            }
            changed = 1;
        }
    }
    return changed;
}
