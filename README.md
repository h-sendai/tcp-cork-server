# TCP CORK serverプログラム

Linux独自の``TCP_CORK``オプションを使った
小さいパケットを連続で送るサーバー。

ただしちゃんと動かないことがある（どうもイーサネットドライバに
依存しそうだ）。

## 起動方法

```
Usage: server [-b bufsize (100)] [-p port (1234)] [-s usec (0)] [-B]
-b bufsize: send data size (100)
-p port:    port number (1234)
-s usec:    usleep usec (don't usleep())
-B:         use busy sleep (excessive CPU consumption)
```

デフォルトでは100バイトのパケットを、スリープなしに送る。
``-s usec``を指定するとwrite()後、usleep(usec)する。

### usleep()に関する注

``-s usec -B``と-Bを追加するとusleep()ではなく
bz_usleep()を使う。bz_usleep()は内部でgettimeofday()を
繰り返し呼び、予定された時刻になったら終了する関数なので
スリープのためにCPUを100%近く消費する。

``-s usec``を指定した場合は``prctl(PR_SET_TIMERSLACK, 1)``を
呼んでいるのでusleep()関数もかなり正確にスリープする。
CPUのC stateがC6などかなりの深度でスリープするようになっている
場合は復帰に時間がかかるようになり復帰のぶんだけよけいにスリープ
することになる。最大C stateをC1にセットするには
``/sys/devices/system/cpu/cpuN/cpuidle/stateM/disable``に対して
echo 1しておく。ここでNはCPU番号、MはCn番号で、名称は
``/sys/devices/system/cpu/cpuN/cpuidle/stateM/name``でわかる。
nameの例:
```
---> cpu0 
cpu0/cpuidle/state0/name:POLL
cpu0/cpuidle/state1/name:C1
cpu0/cpuidle/state2/name:C1E
cpu0/cpuidle/state3/name:C3
cpu0/cpuidle/state4/name:C6
---> cpu1 
cpu1/cpuidle/state0/name:POLL
cpu1/cpuidle/state1/name:C1
cpu1/cpuidle/state2/name:C1E
cpu1/cpuidle/state3/name:C3
cpu1/cpuidle/state4/name:C6
(以下略)
```

### ドライバによるトラフィックの増加

e1000eドライバを使うネットワークインターフェイス間で
通信させると

```
./server -b 200
```

と起動して、イーサネット経由で読み取ると、ほぼイーサネット最大速度
でデータが送られることがあった。
loデバイス経由でデータを読み取ると、そのようなことはなく、
ちゃんと指定したバイト数のデータが送られていることがわかる。

/proc/interruptsで割り込みを見てみると、指定したバイト数の
パケットが来ているときは20000回/秒程度になっていたのが、
イーサネット最大速度でくるときには4000回/秒になっていたので
ドライバの割り込み調整機能が働いたせいだと思う。

ethtool -c NIC名でみるとe1000eの場合はrx-usecsは調整可能だが
tx-usecsはn/aになっているので指定できないようだ。


















