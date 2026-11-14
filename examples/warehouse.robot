# Almacén: cuatro estanterías, una columna y una rampa de carga (zona prohibida).
# Tres AGV llevan pedidos a las estaciones de picking. agv1 y agv2 se cruzan en
# el pasillo central: agv1 tiene prioridad 1 y pasa primero; agv2 espera.
world 14 9 {
  wall 1 1 4 1
  wall 1 6 4 1
  wall 9 1 4 1
  wall 9 6 4 1
  obstacle at 7 7
  hole at 13 4
  robot agv1 at 0 3 facing east priority 1
  robot agv2 at 7 0 facing north priority 2
  robot agv3 at 13 3 facing west priority 3
}
run agv1 {
  forward 7
  left 90
  forward 3
}
run agv2 {
  forward 6
  right 90
  forward 4
}
run agv3 {
  forward 5
  right 90
  forward 3
}