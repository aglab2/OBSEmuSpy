g++ -O2 ./main.cpp -oelevator
sudo mv ./elevator /usr/local/bin/linux_elevator
sudo setcap cap_sys_ptrace=eip /usr/local/bin/linux_elevator
