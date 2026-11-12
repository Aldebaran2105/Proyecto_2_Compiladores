# Dos robots que se cruzan: fusionar avances cambiaría quién bloquea a quién
world 6 3 {
  robot a at 0 1 facing east priority 1
  robot b at 3 0 facing north priority 2
}
run a { forward 1 forward 1 forward 1 }
run b { forward 2 }
