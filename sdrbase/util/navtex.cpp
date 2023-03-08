///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QDebug>
#include <QRegularExpression>

#include "navtex.h"

// From https://en.wikipedia.org/wiki/List_of_Navtex_stations
const QList<NavtexTransmitter> NavtexTransmitter::m_navtexTransmitters = {
    {1, "Svalbard", 78.056944, 13.609722, {Schedule('A', 518000, {QTime(0, 0), QTime(4, 0), QTime(8, 0), QTime(12, 0), QTime(16, 0), QTime(20, 0)})}},
    {1, "Bodo", 67.266667, 14.383333, {NavtexTransmitter::Schedule('B', 518000, {QTime(0, 10), QTime(4, 10), QTime(8, 10), QTime(12, 10), QTime(16, 10), QTime(20, 10)})}},
    {1, "Vardo", 70.370889, 31.097389, {NavtexTransmitter::Schedule('C', 518000, {QTime(0, 20), QTime(4, 20), QTime(8, 20), QTime(12, 20), QTime(16, 20), QTime(20, 20)})}},
    {1, "Torshavn", 62.014944, -6.800056, {NavtexTransmitter::Schedule('D', 518000, {QTime(0, 30), QTime(4, 30), QTime(8, 30), QTime(12, 30), QTime(16, 30), QTime(20, 30)})}},
    {1, "Niton", 50.586297, -1.254756, {NavtexTransmitter::Schedule('E', 518000, {QTime(0, 40), QTime(4, 40), QTime(8, 40), QTime(12, 40), QTime(16, 40), QTime(20, 40)}),
                                        NavtexTransmitter::Schedule('K', 518000, {QTime(1, 40), QTime(5, 40), QTime(6, 40), QTime(13, 40), QTime(17, 40), QTime(21, 40)}),
                                        NavtexTransmitter::Schedule('I', 490000, {QTime(1, 20), QTime(5, 20), QTime(9, 20), QTime(13, 20), QTime(17, 20), QTime(21, 20)})}},  // Have seen this broadcast at 9:10
    {1, "Talinn", 59.4644, 24.357294, {NavtexTransmitter::Schedule('F', 518000, {QTime(3, 20), QTime(7, 20), QTime(11, 20), QTime(15, 20), QTime(19, 20), QTime(23, 20)})}},
    {1, "Cullercoats", 55.0732, -1.463233, {NavtexTransmitter::Schedule('G', 518000, {QTime(1, 0), QTime(5, 0), QTime(9, 0), QTime(13, 0), QTime(17, 0), QTime(21, 0)}),
                                            NavtexTransmitter::Schedule('U', 490000, {QTime(3, 20), QTime(7, 20), QTime(11, 20), QTime(15, 20), QTime(19, 20), QTime(23, 20)})}},
    {1, "Bjuroklubb", 64.461639, 21.591833, {NavtexTransmitter::Schedule('H', 518000, {QTime(1, 10), QTime(5, 10), QTime(9, 10), QTime(13, 10), QTime(17, 10), QTime(21, 10)})}},
    {1, "Grimeton", 57.103056, 12.385556, {NavtexTransmitter::Schedule('I', 518000, {QTime(1, 20), QTime(5, 20), QTime(9, 20), QTime(13, 20), QTime(17, 20), QTime(21, 20)})}},
    {1, "Gislovshammer", 55.488917, 14.314222, {NavtexTransmitter::Schedule('J', 518000, {QTime(1, 30), QTime(5, 30), QTime(9, 30), QTime(13, 30), QTime(17, 30), QTime(21, 30)})}},
    {1, "Rogaland", 58.658817, 5.603778, {NavtexTransmitter::Schedule('L', 518000, {QTime(1, 50), QTime(5, 50), QTime(9, 50), QTime(13, 50), QTime(17, 50), QTime(21, 50)})}},
    {1, "Jeloy", 59.435833, 10.589444, {NavtexTransmitter::Schedule('M', 518000, {QTime(2, 0), QTime(6, 0), QTime(10, 0), QTime(14, 0), QTime(18, 0), QTime(22, 0)})}},
    {1, "Orlandet", 63.661194, 9.5455, {NavtexTransmitter::Schedule('N', 518000, {QTime(2, 10), QTime(6, 10), QTime(10, 10), QTime(14, 10), QTime(18, 10), QTime(22, 10)})}},
    {1, "Portpatrick", 54.844044, -5.124478, {NavtexTransmitter::Schedule('O', 518000, {QTime(2, 20), QTime(6, 20), QTime(10, 20), QTime(14, 20), QTime(18, 20), QTime(22, 20)}),
                                              NavtexTransmitter::Schedule('C', 490000, {QTime(0, 20), QTime(4, 20), QTime(8, 20), QTime(16, 20), QTime(20, 20)})}},
    {1, "Netherlands Coastguard", 52.095128, 4.257975, {NavtexTransmitter::Schedule('P', 518000, {QTime(2, 30), QTime(6, 30), QTime(10, 30), QTime(14, 30), QTime(18, 30), QTime(22, 30)})}},
    {1, "Malin Head", 55.363278, -7.33925, {NavtexTransmitter::Schedule('Q', 518000, {QTime(2, 40), QTime(6, 40), QTime(10, 40), QTime(14, 40), QTime(18, 40), QTime(22, 40)})}},
    {1, "Saudanes", 66.18625, -18.951867, {NavtexTransmitter::Schedule('R', 518000, {QTime(2, 50), QTime(6, 50), QTime(10, 50), QTime(14, 50), QTime(18, 50), QTime(22, 50)}),
                                           NavtexTransmitter::Schedule('E', 490000, {QTime(0, 40), QTime(4, 40), QTime(8, 40), QTime(16, 40), QTime(20, 40)})}},
    {1, "Hamburg", 53.673333, 9.808611, {NavtexTransmitter::Schedule('S', 518000, {QTime(3, 0), QTime(7, 0), QTime(12, 0), QTime(15, 0), QTime(19, 0), QTime(23, 0)}),
                                           NavtexTransmitter::Schedule('L', 490000, {QTime(1, 50), QTime(5, 50), QTime(9, 50), QTime(17, 50), QTime(21, 50)})}},    // Transmitter is at Pinneberg (used on wiki), but messages give location as Hamburg
    {1, "Oostende", 51.182278, 2.806539, {NavtexTransmitter::Schedule('T', 518000, {QTime(3, 10), QTime(7, 10), QTime(11, 10), QTime(15, 10), QTime(19, 10), QTime(23, 10)}),
                                          NavtexTransmitter::Schedule('V', 518000, {QTime(3, 30), QTime(7, 30), QTime(11, 30), QTime(15, 30), QTime(19, 30), QTime(23, 30)}),
                                          NavtexTransmitter::Schedule('B', 490000, {QTime(0, 10), QTime(4, 10), QTime(8, 10), QTime(16, 10), QTime(20, 10)})}},
    {1, "Valentia", 51.929756, -10.349028, {NavtexTransmitter::Schedule('W', 518000, {QTime(3, 40), QTime(7, 40), QTime(12, 40), QTime(15, 40), QTime(19, 40), QTime(23, 40)})}},
    {1, "Grindavik", 63.833208, -22.450786, {NavtexTransmitter::Schedule('X', 518000, {QTime(3, 50), QTime(7, 50), QTime(12, 50), QTime(15, 50), QTime(19, 50), QTime(23, 50)}),
                                            NavtexTransmitter::Schedule('K', 490000, {QTime(1, 40), QTime(5, 40), QTime(9, 40), QTime(17, 40), QTime(21, 40)})}},

    {2, "Cross Corsen", 48.476031, -5.053697, {NavtexTransmitter::Schedule('A', 518000, {QTime(0, 0), QTime(4, 0), QTime(8, 0), QTime(12, 0), QTime(16, 0), QTime(20, 0)}),
                                               NavtexTransmitter::Schedule('E', 490000, {QTime(0, 40), QTime(4, 40), QTime(8, 40), QTime(12, 40), QTime(16, 40), QTime(20, 40)})}},
    {2, "Coruna", 43.367028, -8.451861, {NavtexTransmitter::Schedule('D', 518000, {QTime(0, 30), QTime(4, 30), QTime(8, 30), QTime(12, 30), QTime(16, 30), QTime(20, 30)}),
                                         NavtexTransmitter::Schedule('W', 490000, {QTime(3, 40), QTime(7, 40), QTime(11, 40), QTime(15, 40), QTime(17, 40), QTime(23, 40)})}},
    {2, "Horta", 38.529872, -28.628922, {NavtexTransmitter::Schedule('F', 518000, {QTime(0, 50), QTime(4, 50), QTime(8, 50), QTime(12, 50), QTime(16, 50), QTime(20, 50)}),
                                         NavtexTransmitter::Schedule('J', 490000, {QTime(1, 30), QTime(5, 30), QTime(9, 30), QTime(13, 30), QTime(15, 30), QTime(21, 30)})}},
    {2, "Tarifa", 36.042, -5.556606, {NavtexTransmitter::Schedule('G', 518000, {QTime(1, 0), QTime(5, 0), QTime(9, 0), QTime(13, 0), QTime(17, 0), QTime(21, 0)})}},
    {2, "Las Palmas", 27.758522, -15.605361, {NavtexTransmitter::Schedule('I', 518000, {QTime(1, 20), QTime(5, 20), QTime(9, 20), QTime(13, 20), QTime(17, 20), QTime(21, 20)}),
                                              NavtexTransmitter::Schedule('A', 490000, {QTime(0, 0), QTime(4, 0), QTime(8, 0), QTime(12, 0), QTime(16, 0), QTime(20, 0)})}},
    {2, "Casablanca", 33.6, -7.633333, {NavtexTransmitter::Schedule('M', 518000, {QTime(2, 0), QTime(6, 0), QTime(10, 0), QTime(14, 0), QTime(18, 0), QTime(22, 0)})}},
    {2, "Porto Santo", 33.066278, -16.355417, {NavtexTransmitter::Schedule('P', 518000, {QTime(2, 30), QTime(6, 30), QTime(10, 30), QTime(14, 30), QTime(18, 30), QTime(22, 30)})}},
    {2, "Monsanto", 38.731611, -9.190611, {NavtexTransmitter::Schedule('R', 518000, {QTime(2, 50), QTime(6, 50), QTime(10, 50), QTime(14, 50), QTime(18, 50), QTime(22, 50)}),
                                           NavtexTransmitter::Schedule('G', 490000, {QTime(1, 0), QTime(5, 0), QTime(9, 0), QTime(13, 0), QTime(15, 0), QTime(21, 0)})}},
    {2, "Ribeira de Vinha", 16.853228, -25.003197, {NavtexTransmitter::Schedule('U', 518000, {QTime(3, 20), QTime(7, 20), QTime(11, 20), QTime(15, 20), QTime(19, 20), QTime(23, 20)})}},
    {2, "Cabo La Nao", 38.723258, 0.161367, {NavtexTransmitter::Schedule('X', 518000, {QTime(3, 50), QTime(7, 520), QTime(11, 50), QTime(15, 50), QTime(19, 50), QTime(23, 50)}),
                                             NavtexTransmitter::Schedule('M', 490000, {QTime(2, 0), QTime(6, 0), QTime(10, 0), QTime(14, 0), QTime(16, 0), QTime(22, 0)})}},
    {2, "Sao Vicente", 16.853228, -25.003197, {NavtexTransmitter::Schedule('P', 490000, {QTime(2, 30), QTime(6, 30), QTime(10, 30), QTime(14, 30), QTime(16, 30), QTime(22, 30)})}},
//    {2, "Niton", 50.586297, -1.254756, {NavtexTransmitter::Schedule('T', 490000, {QTime(3, 10), QTime(7, 10), QTime(11, 10), QTime(15, 10), QTime(17, 10), QTime(23, 10)})}},
    {2, "Tarifa", 36.042, -5.556606, {NavtexTransmitter::Schedule('T', 490000, {QTime(3, 10), QTime(7, 10), QTime(11, 10), QTime(15, 10), QTime(17, 10), QTime(23, 10)})}},

    {3, "Novorossijsk", 44.599111, 37.951442, {NavtexTransmitter::Schedule('A', 518000, {QTime(0, 0), QTime(4, 0), QTime(8, 0), QTime(12, 0), QTime(16, 0), QTime(20, 0)})}},
    {3, "Algier", 36.733333, 3.18, {NavtexTransmitter::Schedule('B', 518000, {QTime(0, 10), QTime(4, 10), QTime(8, 10), QTime(12, 10), QTime(16, 10), QTime(20, 10)})}},
    {3, "Odessa", 46.377611, 30.748222, {NavtexTransmitter::Schedule('C', 518000, {QTime(2, 30), QTime(6, 30), QTime(10, 30), QTime(14, 30), QTime(18, 30), QTime(22, 30)})}},
    {3, "Istanbul", 41.066667, 28.95, {NavtexTransmitter::Schedule('D', 518000, {QTime(0, 30), QTime(4, 30), QTime(8, 30), QTime(12, 30), QTime(16, 30), QTime(20, 30)}),
                                       NavtexTransmitter::Schedule('B', 490000, {QTime(0, 10), QTime(4, 10), QTime(8, 10), QTime(16, 10), QTime(20, 10)}),
                                       NavtexTransmitter::Schedule('M', 4209500, {QTime(2, 0), QTime(6, 0), QTime(10, 0), QTime(14, 0), QTime(16, 0), QTime(22, 0)})}},
    {3, "Samsun", 41.386667, 36.188333, {NavtexTransmitter::Schedule('E', 518000, {QTime(0, 40), QTime(4, 40), QTime(8, 40), QTime(12, 40), QTime(16, 40), QTime(20, 40)}),
                                         NavtexTransmitter::Schedule('A', 490000, {QTime(0, 0), QTime(4, 0), QTime(8, 0), QTime(12, 0), QTime(16, 0), QTime(20, 0)})}},
    {3, "Antalya", 36.1525, 32.44, {NavtexTransmitter::Schedule('F', 518000, {QTime(0, 50), QTime(4, 50), QTime(8, 50), QTime(12, 50), QTime(16, 50), QTime(20, 50)}),
                                    NavtexTransmitter::Schedule('D', 490000, {QTime(0, 30), QTime(4, 30), QTime(8, 30), QTime(16, 30), QTime(20, 30)})}},
    {3, "Iraklio", 35.322861, 25.748986, {NavtexTransmitter::Schedule('H', 518000, {QTime(1, 10), QTime(5, 10), QTime(9, 10), QTime(13, 10), QTime(17, 10), QTime(21, 10)}),
                                          NavtexTransmitter::Schedule('Q', 490000, {QTime(2, 40), QTime(6, 40), QTime(10, 40), QTime(14, 40), QTime(16, 40), QTime(22, 40)}),
                                          NavtexTransmitter::Schedule('S', 4209500, {QTime(3, 0), QTime(7, 0), QTime(11, 0), QTime(15, 0), QTime(19, 0), QTime(3, 20)})}},
    {3, "Izmir", 38.275833, 26.2675, {NavtexTransmitter::Schedule('I', 518000, {QTime(1, 20), QTime(5, 20), QTime(9, 20), QTime(13, 20), QTime(17, 20), QTime(21, 20)}),
                                      NavtexTransmitter::Schedule('C', 490000, {QTime(0, 20), QTime(4, 20), QTime(8, 20), QTime(16, 20), QTime(20, 20)})}},
    {3, "Varna", 43.068056, 27.786111, {NavtexTransmitter::Schedule('J', 518000, {QTime(1, 30), QTime(5, 30), QTime(9, 30), QTime(13, 30), QTime(17, 30), QTime(21, 30)})}},
    {3, "Kerkyra", 39.607222, 19.890833, {NavtexTransmitter::Schedule('K', 518000, {QTime(1, 40), QTime(5, 40), QTime(9, 40), QTime(13, 40), QTime(17, 40), QTime(21, 40)}),
                                          NavtexTransmitter::Schedule('P', 490000, {QTime(2, 30), QTime(6, 30), QTime(10, 30), QTime(14, 30), QTime(16, 30), QTime(22, 30)})}},
    {3, "Limnos", 39.906389, 25.181389, {NavtexTransmitter::Schedule('L', 518000, {QTime(1, 50), QTime(5, 50), QTime(9, 50), QTime(13, 50), QTime(17, 50), QTime(21, 50)}),
                                         NavtexTransmitter::Schedule('R', 490000, {QTime(2, 50), QTime(6, 50), QTime(10, 50), QTime(14, 50), QTime(16, 50), QTime(22, 50)})}},
    {3, "Cyprus", 35.048278, 33.283628, {NavtexTransmitter::Schedule('M', 518000, {QTime(2, 0), QTime(6, 0), QTime(10, 0), QTime(14, 0), QTime(18, 0), QTime(22, 0)})}},
    {3, "Alexandria", 31.198089, 29.864494, {NavtexTransmitter::Schedule('N', 518000, {QTime(2, 10), QTime(6, 10), QTime(10, 10), QTime(14, 10), QTime(18, 10), QTime(22, 10)})}},
    {3, "Malta", 35.815211, 14.526911, {NavtexTransmitter::Schedule('O', 518000, {QTime(2, 20), QTime(6, 20), QTime(10, 20), QTime(14, 20), QTime(18, 20), QTime(22, 20)})}},
    {3, "Haifa", 32.827806, 34.969306, {NavtexTransmitter::Schedule('P', 518000, {QTime(2, 30), QTime(6, 30), QTime(10, 30), QTime(14, 30), QTime(18, 30), QTime(22, 30)})}},
    {3, "Split", 43.181861, 16.422333, {NavtexTransmitter::Schedule('Q', 518000, {QTime(2, 40), QTime(6, 40), QTime(10, 40), QTime(14, 40), QTime(18, 40), QTime(22, 40)})}},
    {3, "La Maddalena", 41.222778, 9.398889, {NavtexTransmitter::Schedule('R', 518000, {QTime(2, 50), QTime(6, 50), QTime(10, 50), QTime(14, 50), QTime(18, 50), QTime(22, 50)}),
                                              NavtexTransmitter::Schedule('I', 490000, {QTime(1, 20), QTime(5, 20), QTime(9, 20), QTime(13, 20), QTime(17, 20), QTime(21, 20)})}},
    {3, "Kelibia", 36.801819, 11.037372, {NavtexTransmitter::Schedule('T', 518000, {QTime(3, 10), QTime(7, 10), QTime(12, 10), QTime(15, 10), QTime(19, 10), QTime(23, 10)})}},
    {3, "Mondolfo", 43.747778, 13.141667, {NavtexTransmitter::Schedule('U', 518000, {QTime(3, 20), QTime(7, 20), QTime(12, 20), QTime(15, 20), QTime(19, 20), QTime(23, 20)}),
                                           NavtexTransmitter::Schedule('E', 490000, {QTime(0, 40), QTime(4, 40), QTime(8, 40), QTime(12, 40), QTime(16, 40), QTime(20, 40)})}},
    {3, "Sellia Marina", 38.873056, 16.719722, {NavtexTransmitter::Schedule('V', 518000, {QTime(3, 30), QTime(7, 30), QTime(12, 30), QTime(15, 30), QTime(19, 30), QTime(23, 30)}),
                                                NavtexTransmitter::Schedule('W', 490000, {QTime(3, 40), QTime(7, 40), QTime(11, 40), QTime(15, 40), QTime(17, 40), QTime(23, 40)})}},
    {3, "Cross La Garde", 43.104306, 5.991389, {NavtexTransmitter::Schedule('W', 518000, {QTime(3, 40), QTime(7, 40), QTime(12, 40), QTime(15, 40), QTime(19, 40), QTime(23, 40)}),
                                                NavtexTransmitter::Schedule('S', 490000, {QTime(3, 0), QTime(7, 0), QTime(11, 0), QTime(15, 0), QTime(19, 0), QTime(3, 20)})}},
    {3, "Cabo de la Nao", 38.723258, 0.161367, {NavtexTransmitter::Schedule('X', 518000, {QTime(3, 50), QTime(7, 50), QTime(12, 50), QTime(15, 50), QTime(19, 50), QTime(23, 50)}),
                                                NavtexTransmitter::Schedule('M', 490000, {QTime(2, 0), QTime(6, 0), QTime(10, 0), QTime(14, 0), QTime(16, 0), QTime(22, 0)})}},

    {4, "Miami", 25.626225, -80.383411, {NavtexTransmitter::Schedule('A', 518000, {QTime(0, 0), QTime(4, 0), QTime(8, 0), QTime(12, 0), QTime(16, 0), QTime(20, 0)})}},
    {4, "Bermuda Harbour", 32.380389, -64.682778, {NavtexTransmitter::Schedule('B', 518000, {QTime(0, 10), QTime(4, 10), QTime(8, 10), QTime(12, 10), QTime(16, 10), QTime(20, 10)})}},
    {4, "Riviere-au-Renard", 50.195, -66.109889, {NavtexTransmitter::Schedule('C', 518000, {QTime(0, 20), QTime(4, 20), QTime(8, 20), QTime(12, 20), QTime(16, 20), QTime(20, 20)}),
                                                  NavtexTransmitter::Schedule('D', 490000, {QTime(0, 30), QTime(4, 30), QTime(8, 30), QTime(16, 30), QTime(20, 30)})}},
    {4, "Boston", 41.709833, -70.498353, {NavtexTransmitter::Schedule('F', 518000, {QTime(0, 50), QTime(4, 20), QTime(8, 50), QTime(12, 50), QTime(16, 50), QTime(20, 50)})}},
    {4, "New Orleans", 29.884625, -89.945611, {NavtexTransmitter::Schedule('G', 518000, {QTime(1, 0), QTime(5, 0), QTime(9, 0), QTime(13, 0), QTime(17, 0), QTime(21, 0)}),
                                               NavtexTransmitter::Schedule('G', 4209500, {QTime(3, 0), QTime(7, 0), QTime(11, 0), QTime(15, 0), QTime(19, 0), QTime(23, 0)})}},
    {4, "Wiarton", 44.937111, -81.233467, {NavtexTransmitter::Schedule('H', 518000, {QTime(1, 10), QTime(5, 10), QTime(9, 10), QTime(13, 10), QTime(17, 10), QTime(21, 10)})}},
    {4, "Curacao", 12.173197, -68.864919, {NavtexTransmitter::Schedule('H', 518000, {QTime(1, 10), QTime(5, 10), QTime(9, 10), QTime(13, 10), QTime(17, 10), QTime(21, 10)})}},          //  Duplicate Id
    {4, "Portsmouth", 36.726342, -76.007894, {NavtexTransmitter::Schedule('N', 518000, {QTime(2, 10), QTime(6, 10), QTime(10, 10), QTime(14, 10), QTime(18, 10), QTime(22, 10)})}},
    {4, "St. John's", 47.611111, -52.666944, {NavtexTransmitter::Schedule('O', 518000, {QTime(2, 20), QTime(6, 20), QTime(10, 20), QTime(14, 20), QTime(18, 20), QTime(22, 20)})}},
    {4, "Thunder Bay", 48.563514, -88.656311, {NavtexTransmitter::Schedule('P', 518000, {QTime(2, 30), QTime(6, 30), QTime(10, 30), QTime(14, 30), QTime(18, 30), QTime(22, 30)})}},
    {4, "Sydney", 46.185556, -59.893611, {NavtexTransmitter::Schedule('Q', 518000, {QTime(2, 40), QTime(6, 40), QTime(10, 40), QTime(14, 40), QTime(18, 40), QTime(22, 40)}),
                                          NavtexTransmitter::Schedule('J', 490000, {QTime(1, 30), QTime(5, 30), QTime(9, 30), QTime(13, 30), QTime(15, 30), QTime(21, 30)})}},

    {4, "Isabela", 18.466683, -67.071819, {NavtexTransmitter::Schedule('R', 518000, {QTime(2, 50), QTime(6, 50), QTime(10, 50), QTime(14, 50), QTime(18, 50), QTime(22, 50)})}},
    {4, "Iqaluit", 63.731389, -68.543167, {NavtexTransmitter::Schedule('T', 518000, {QTime(3, 10), QTime(7, 10), QTime(11, 10), QTime(15, 10), QTime(19, 10), QTime(23, 10)}),
                                           NavtexTransmitter::Schedule('S', 490000, {QTime(3, 0), QTime(7, 0), QTime(11, 0), QTime(15, 0), QTime(19, 0), QTime(3, 20)})}},
    {4, "Saint John", 43.744256, -66.121786, {NavtexTransmitter::Schedule('U', 518000, {QTime(3, 20), QTime(7, 20), QTime(11, 20), QTime(15, 20), QTime(19, 20), QTime(23, 20)}),
                                              NavtexTransmitter::Schedule('V', 490000, {QTime(3, 30), QTime(7, 30), QTime(11, 30), QTime(15, 30), QTime(19, 30), QTime(23, 30)})}},
    {4, "Kook Island", 64.067017, -52.012611, {NavtexTransmitter::Schedule('W', 518000, {QTime(3, 40), QTime(7, 40), QTime(12, 40), QTime(15, 40), QTime(19, 40), QTime(23, 40)})}},
    {4, "Labrador", 53.708611, -57.021667, {NavtexTransmitter::Schedule('X', 518000, {QTime(3, 50), QTime(7, 50), QTime(12, 50), QTime(15, 50), QTime(19, 50), QTime(23, 50)})}},

    {6, "La Paloma", -34.666667, -54.15, {NavtexTransmitter::Schedule('F', 518000, {QTime(0, 50), QTime(4, 50), QTime(8, 50), QTime(12, 50), QTime(16, 50), QTime(20, 50)})}},
    {6, "Ushuaia", -54.8, -68.3, {NavtexTransmitter::Schedule('M', 518000, {QTime(2, 0), QTime(6, 0), QTime(10, 0), QTime(14, 0), QTime(18, 0), QTime(22, 0)})}},
    {6, "Rio Gallegos", -51.616667, -69.216667, {NavtexTransmitter::Schedule('N', 518000, {QTime(2, 10), QTime(6, 10), QTime(10, 10), QTime(14, 10), QTime(18, 10), QTime(22, 10)})}},
    {6, "Comodoro Rivadavia", -45.85, -67.416667, {NavtexTransmitter::Schedule('O', 518000, {QTime(2, 20), QTime(6, 20), QTime(10, 20), QTime(14, 20), QTime(18, 20), QTime(22, 20)})}},
    {6, "Bahía Blanca", -38.716667, -62.1, {NavtexTransmitter::Schedule('P', 518000, {QTime(2, 30), QTime(6, 30), QTime(10, 30), QTime(14, 30), QTime(18, 30), QTime(22, 30)})}},
    {6, "Mar del Plata", -38.05, -57.533333, {NavtexTransmitter::Schedule('Q', 518000, {QTime(2, 40), QTime(6, 40), QTime(10, 40), QTime(14, 40), QTime(18, 40), QTime(22, 40)})}},
    {6, "Buenos Aires", -34.6, -58.366667, {NavtexTransmitter::Schedule('R', 518000, {QTime(2, 50), QTime(6, 50), QTime(10, 50), QTime(14, 50), QTime(18, 50), QTime(22, 50)})}},

    {7, "Walvis Bay", -23.05665, 14.624333, {NavtexTransmitter::Schedule('B', 518000, {QTime(0, 10), QTime(4, 10), QTime(8, 10), QTime(12, 10), QTime(16, 10), QTime(20, 10)})}},
    {7, "Cape Town", -33.685128, 18.712961, {NavtexTransmitter::Schedule('C', 518000, {QTime(0, 20), QTime(4, 20), QTime(8, 20), QTime(12, 20), QTime(16, 20), QTime(20, 20)})}},
    {7, "Port Elizabeth", -34.036722, 25.555833, {NavtexTransmitter::Schedule('I', 518000, {QTime(1, 20), QTime(5, 20), QTime(9, 20), QTime(13, 20), QTime(17, 20), QTime(21, 20)})}},
    {7, "Durban", -29.804833, 30.815633, {NavtexTransmitter::Schedule('O', 518000, {QTime(2, 20), QTime(6, 20), QTime(10, 20), QTime(14, 20), QTime(18, 20), QTime(22, 20)})}},

    {8, "Mauritius", -20.167089, 57.478161, {NavtexTransmitter::Schedule('C', 518000, {QTime(0, 20), QTime(4, 20), QTime(8, 20), QTime(12, 20), QTime(16, 20), QTime(20, 20)})}},
    {8, "Bombay", 19.083239, 72.834033, {NavtexTransmitter::Schedule('G', 518000, {QTime(1, 0), QTime(5, 0), QTime(9, 0), QTime(13, 0), QTime(17, 0), QTime(21, 0)})}},
    {8, "Madras", 13.082778, 80.287222, {NavtexTransmitter::Schedule('P', 518000, {QTime(2, 30), QTime(6, 30), QTime(10, 30), QTime(14, 30), QTime(18, 30), QTime(22, 30)})}},

    {9, "Bushehr", 28.962225, 50.822794, {NavtexTransmitter::Schedule('A', 518000, {QTime(0, 0), QTime(4, 0), QTime(8, 0), QTime(12, 0), QTime(16, 0), QTime(20, 0)})}},
    {9, "Hamala", 26.157167, 50.47665, {NavtexTransmitter::Schedule('B', 518000, {QTime(0, 10), QTime(4, 10), QTime(8, 10), QTime(12, 10), QTime(16, 10), QTime(20, 10)})}},
    {9, "Bandar Abbas", 27.161022, 56.225378, {NavtexTransmitter::Schedule('F', 518000, {QTime(0, 50), QTime(4, 50), QTime(8, 50), QTime(12, 50), QTime(16, 50), QTime(20, 50)})}},
    {9, "Jeddah", 21.342222, 39.155833, {NavtexTransmitter::Schedule('H', 518000, {QTime(1, 10), QTime(5, 10), QTime(9, 10), QTime(13, 10), QTime(17, 10), QTime(21, 10)})}},
    {9, "Muscat", 23.6, 58.5, {NavtexTransmitter::Schedule('M', 518000, {QTime(2, 0), QTime(6, 0), QTime(10, 0), QTime(14, 0), QTime(18, 0), QTime(22, 0)})}},
    {9, "Karachi", 24.851944, 67.0425, {NavtexTransmitter::Schedule('P', 518000, {QTime(2, 30), QTime(6, 30), QTime(10, 30), QTime(14, 30), QTime(18, 30), QTime(22, 30)})}},
    {9, "Quseir", 26.110889, 34.280083, {NavtexTransmitter::Schedule('V', 518000, {QTime(3, 30), QTime(7, 30), QTime(11, 30), QTime(15, 30), QTime(19, 30), QTime(23, 30)})}},
    {9, "Serapeum ", 30.470311, 32.36675, {NavtexTransmitter::Schedule('X', 518000, {QTime(3, 50), QTime(7, 50), QTime(12, 50), QTime(15, 50), QTime(19, 50), QTime(23, 50)})}},

    {11, "Jayapura", -2.516667, 140.716667, {NavtexTransmitter::Schedule('A', 518000, {QTime(0, 0), QTime(4, 0), QTime(8, 0), QTime(12, 0), QTime(16, 0), QTime(20, 0)})}},
    {11, "Ambon", -3.7, 128.2, {NavtexTransmitter::Schedule('B', 518000, {QTime(0, 10), QTime(4, 10), QTime(8, 10), QTime(12, 10), QTime(16, 10), QTime(20, 10)})}},
    {11, "Singapore", 1.333333, 103.7, {NavtexTransmitter::Schedule('C', 518000, {QTime(0, 20), QTime(4, 20), QTime(8, 20), QTime(12, 20), QTime(16, 20), QTime(20, 20)})}},
    {11, "Makassar", -5.1, 119.433333, {NavtexTransmitter::Schedule('D', 518000, {QTime(0, 30), QTime(4, 30), QTime(8, 30), QTime(12, 30), QTime(16, 30), QTime(20, 30)})}},
    {11, "Jakarta", -6.116667, 106.866667, {NavtexTransmitter::Schedule('E', 518000, {QTime(0, 40), QTime(4, 40), QTime(8, 40), QTime(12, 40), QTime(16, 40), QTime(20, 40)})}},
    {11, "Bangkok", 13.024444, 100.019733, {NavtexTransmitter::Schedule('F', 518000, {QTime(0, 50), QTime(4, 50), QTime(8, 50), QTime(12, 50), QTime(16, 50), QTime(20, 50)})}},
    {11, "Naha", 26.15, 127.766667, {NavtexTransmitter::Schedule('G', 518000, {QTime(1, 0), QTime(5, 0), QTime(9, 0), QTime(13, 0), QTime(17, 0), QTime(21, 0)})}},
    {11, "Moji", 33.95, 130.966667, {NavtexTransmitter::Schedule('H', 518000, {QTime(1, 10), QTime(5, 10), QTime(9, 10), QTime(13, 10), QTime(17, 10), QTime(21, 10)})}},
    {11, "Puerto Princesa", 9.733333, 118.716667, {NavtexTransmitter::Schedule('I', 518000, {QTime(1, 20), QTime(5, 20), QTime(9, 20), QTime(13, 20), QTime(17, 20), QTime(21, 20)})}},
    {11, "Yokohama", 35.433333, 139.633333, {NavtexTransmitter::Schedule('I', 518000, {QTime(1, 20), QTime(5, 20), QTime(9, 20), QTime(13, 20), QTime(17, 20), QTime(21, 20)})}},  // Duplicate Id
    {11, "Manila", 14.583333, 121.05, {NavtexTransmitter::Schedule('J', 518000, {QTime(1, 30), QTime(5, 30), QTime(9, 30), QTime(13, 30), QTime(17, 30), QTime(21, 30)})}},
    {11, "Otaru", 43.2, 141, {NavtexTransmitter::Schedule('J', 518000, {QTime(1, 30), QTime(5, 30), QTime(9, 30), QTime(13, 30), QTime(17, 30), QTime(21, 30)})}}, // Duplicate Id
    {11, "Davao City", 7.066667, 125.6, {NavtexTransmitter::Schedule('K', 518000, {QTime(1, 40), QTime(5, 40), QTime(9, 40), QTime(13, 40), QTime(17, 40), QTime(21, 40)})}},
    {11, "Kushiro", 42.983333, 144.383333, {NavtexTransmitter::Schedule('K', 518000, {QTime(1, 40), QTime(5, 40), QTime(9, 40), QTime(13, 40), QTime(17, 40), QTime(21, 40)})}},  // Duplicate Id
    {11, "Hongkong", 22.209167, 114.256111, {NavtexTransmitter::Schedule('L', 518000, {QTime(1, 50), QTime(5, 50), QTime(9, 50), QTime(13, 50), QTime(17, 50), QTime(21, 50)})}},
    {11, "Sanya", 18.232222, 109.495833, {NavtexTransmitter::Schedule('M', 518000, {QTime(2, 0), QTime(6, 0), QTime(10, 0), QTime(14, 0), QTime(18, 0), QTime(22, 0)})}},
    {11, "Guangzhou", 23.15, 113.483333, {NavtexTransmitter::Schedule('N', 518000, {QTime(2, 10), QTime(6, 10), QTime(10, 10), QTime(14, 10), QTime(18, 10), QTime(22, 10)})}},
    {11, "Fuzhou", 26.028544, 119.305444, {NavtexTransmitter::Schedule('O', 518000, {QTime(2, 20), QTime(6, 20), QTime(10, 20), QTime(14, 20), QTime(18, 20), QTime(22, 20)})}},
    {11, "Da Nang", 16.083333, 108.233333, {NavtexTransmitter::Schedule('P', 518000, {QTime(2, 30), QTime(6, 30), QTime(10, 30), QTime(14, 30), QTime(18, 30), QTime(22, 30)})}},
    {11, "Chilung", 25.15, 121.733333, {NavtexTransmitter::Schedule('P', 518000, {QTime(2, 30), QTime(6, 30), QTime(10, 30), QTime(14, 30), QTime(18, 30), QTime(22, 30)})}}, // Duplicate Id
    {11, "Shanghai", 31.108889, 121.544167, {NavtexTransmitter::Schedule('Q', 518000, {QTime(2, 40), QTime(6, 40), QTime(10, 40), QTime(14, 40), QTime(18, 40), QTime(22, 40)})}},
    {11, "Dalian", 38.845244, 121.518056, {NavtexTransmitter::Schedule('R', 518000, {QTime(2, 50), QTime(6, 50), QTime(10, 50), QTime(14, 50), QTime(18, 50), QTime(22, 50)})}},
    {11, "Sandakan", 5.895886, 118.00305, {NavtexTransmitter::Schedule('S', 518000, {QTime(3, 0), QTime(7, 0), QTime(12, 0), QTime(15, 0), QTime(19, 0), QTime(23, 0)})}},
    {11, "Miri", 4.438, 114.020889, {NavtexTransmitter::Schedule('T', 518000, {QTime(3, 10), QTime(7, 10), QTime(11, 10), QTime(15, 10), QTime(19, 10), QTime(23, 10)})}},
    {11, "Penang", 5.425, 100.403056, {NavtexTransmitter::Schedule('U', 518000, {QTime(3, 20), QTime(7, 20), QTime(11, 20), QTime(15, 20), QTime(19, 20), QTime(23, 20)})}},
    {11, "Guam", 13.47445, 144.844389, {NavtexTransmitter::Schedule('V', 518000, {QTime(3, 30), QTime(7, 30), QTime(11, 30), QTime(15, 30), QTime(19, 30), QTime(23, 30)})}},
    {11, "Jukbyeon", 37.05, 129.416667, {NavtexTransmitter::Schedule('V', 518000, {QTime(3, 30), QTime(7, 30), QTime(11, 30), QTime(15, 30), QTime(19, 30), QTime(23, 30)}),  // Duplicate Id
                                         NavtexTransmitter::Schedule('J', 490000, {QTime(1, 30), QTime(5, 30), QTime(9, 30), QTime(13, 30), QTime(15, 30), QTime(21, 30)})}},

    {11, "Byeonsan", 35.6, 126.483333, {NavtexTransmitter::Schedule('W', 518000, {QTime(3, 40), QTime(7, 40), QTime(12, 40), QTime(15, 40), QTime(19, 40), QTime(23, 40)}),
                                        NavtexTransmitter::Schedule('K', 490000, {QTime(1, 40), QTime(5, 40), QTime(9, 40), QTime(17, 40), QTime(21, 40)})}},
    {11, "Ho-Chi-Minh City", 10.703317, 106.729139, {NavtexTransmitter::Schedule('X', 518000, {QTime(3, 50), QTime(7, 50), QTime(12, 50), QTime(15, 50), QTime(19, 50), QTime(23, 50)})}},

    {12, "San Francisco", 37.925739, -122.734056, {NavtexTransmitter::Schedule('C', 518000, {QTime(0, 20), QTime(4, 20), QTime(8, 20), QTime(12, 20), QTime(16, 20), QTime(20, 20)})}},
    {12, "Prince Rupert", 54.298519, -130.417669, {NavtexTransmitter::Schedule('D', 518000, {QTime(0, 30), QTime(4, 30), QTime(8, 30), QTime(12, 30), QTime(16, 30), QTime(20, 30)})}},
    {12, "Tofino", 48.925478, -125.540306, {NavtexTransmitter::Schedule('H', 518000, {QTime(1, 10), QTime(5, 10), QTime(9, 10), QTime(13, 10), QTime(17, 10), QTime(21, 10)})}},
    {12, "Kodiak", 57.781606, -152.537583, {NavtexTransmitter::Schedule('J', 518000, {QTime(1, 30), QTime(5, 30), QTime(9, 30), QTime(13, 30), QTime(17, 30), QTime(21, 30)}),
                                            NavtexTransmitter::Schedule('X', 518000, {QTime(3, 50), QTime(7, 50), QTime(12, 50), QTime(15, 50), QTime(19, 50), QTime(23, 50)})}},
    {12, "Ayora", -0.75, -90.316667, {NavtexTransmitter::Schedule('L', 518000, {QTime(1, 50), QTime(5, 50), QTime(9, 50), QTime(13, 50), QTime(17, 50), QTime(21, 50)}),
                                      NavtexTransmitter::Schedule('A', 490000, {QTime(0, 0), QTime(4, 0), QTime(8, 0), QTime(12, 0), QTime(16, 0), QTime(20, 0)})}},
    {12, "Guayaquil", -2.283333, -80.016667, {NavtexTransmitter::Schedule('M', 518000, {QTime(2, 0), QTime(6, 0), QTime(10, 0), QTime(14, 0), QTime(18, 0), QTime(22, 0)})}},
    {12, "Honolulu", 21.437019, -158.143239, {NavtexTransmitter::Schedule('O', 518000, {QTime(2, 20), QTime(6, 20), QTime(10, 20), QTime(14, 20), QTime(18, 20), QTime(22, 20)})}},
    {12, "Cambria", 35.524297, -121.061922, {NavtexTransmitter::Schedule('Q', 518000, {QTime(2, 40), QTime(6, 40), QTime(10, 40), QTime(14, 40), QTime(18, 40), QTime(22, 40)})}},
    {12, "Astoria", 46.203989, -123.955639, {NavtexTransmitter::Schedule('W', 518000, {QTime(3, 40), QTime(7, 40), QTime(12, 40), QTime(15, 40), QTime(19, 40), QTime(23, 40)})}},

    {13, "Vladivostok", 43.381472, 131.899861, {NavtexTransmitter::Schedule('A', 518000, {QTime(0, 0), QTime(4, 0), QTime(8, 0), QTime(12, 0), QTime(16, 0), QTime(20, 0)})}},
    {13, "Kholmsk", 47.023556, 142.045056, {NavtexTransmitter::Schedule('B', 518000, {QTime(0, 10), QTime(4, 10), QTime(8, 10), QTime(12, 10), QTime(16, 10), QTime(20, 10)})}},
    {13, "Murmansk", 68.865803, 33.070761, {NavtexTransmitter::Schedule('C', 518000, {QTime(0, 20), QTime(4, 20), QTime(8, 20), QTime(12, 20), QTime(16, 20), QTime(20, 20)})}},
    {13, "Petropavlosk", 53.247778, 158.419472, {NavtexTransmitter::Schedule('C', 518000, {QTime(0, 20), QTime(4, 20), QTime(8, 20), QTime(12, 20), QTime(16, 20), QTime(20, 20)})}},        // Duplicate
    {13, "Magadan", 59.683333, 150.15, {NavtexTransmitter::Schedule('D', 518000, {QTime(0, 30), QTime(4, 30), QTime(8, 30), QTime(12, 30), QTime(16, 30), QTime(20, 30)})}},
    {13, "Archangelsk", 64.556278, 40.550028, {NavtexTransmitter::Schedule('F', 518000, {QTime(0, 50), QTime(4, 50), QTime(8, 50), QTime(12, 50), QTime(16, 50), QTime(20, 50)})}},
    {13, "Okhotsk", 59.366667, 143.2, {NavtexTransmitter::Schedule('G', 518000, {QTime(1, 0), QTime(5, 0), QTime(9, 0), QTime(13, 0), QTime(17, 0), QTime(21, 0)})}},
    {13, "Astrakhan", 46.296694, 47.997778, {NavtexTransmitter::Schedule('W', 518000, {QTime(3, 40), QTime(7, 40), QTime(12, 40), QTime(15, 40), QTime(19, 40), QTime(23, 40)})}},

    {15, "Antofagasta", -23.491333, -70.424778, {NavtexTransmitter::Schedule('A', 518000, {QTime(4, 0), QTime(12, 0), QTime(20, 0)})}},
    {15, "Valparaíso", -32.802222, -71.485, {NavtexTransmitter::Schedule('B', 518000, {QTime(4, 10), QTime(12, 10), QTime(20, 10)})}},
    {15, "Talcahuano", -36.715056, -73.108, {NavtexTransmitter::Schedule('C', 518000, {QTime(4, 20), QTime(12, 20), QTime(20, 20)})}},
    {15, "Puerto Montt", -41.489983, -72.957744, {NavtexTransmitter::Schedule('D', 518000, {QTime(4, 30), QTime(12, 30), QTime(20, 30)})}},
    {15, "Punta Arenas", -52.948111, -71.056944, {NavtexTransmitter::Schedule('E', 518000, {QTime(4, 40), QTime(12, 40), QTime(20, 40)})}},
    {15, "Easter Island", -27.15, -109.416667, {NavtexTransmitter::Schedule('F', 518000, {QTime(4, 50), QTime(12, 50), QTime(20, 50)})}},

    {16, "Paita", -5.083333, -81.116667, {NavtexTransmitter::Schedule('S', 518000, {QTime(3, 0), QTime(7, 0), QTime(12, 0), QTime(15, 0), QTime(19, 0), QTime(23, 0)})}},
    {16, "Callao", -12.5, -77.15, {NavtexTransmitter::Schedule('U', 518000, {QTime(3, 20), QTime(7, 20), QTime(11, 20), QTime(15, 20), QTime(19, 20), QTime(23, 20)})}},
    {16, "Mollendo", -17.016667, -72.016667, {NavtexTransmitter::Schedule('W', 518000, {QTime(3, 40), QTime(7, 40), QTime(12, 40), QTime(15, 40), QTime(19, 40), QTime(23, 40)})}},

};

const QMap<QString,QString> NavtexMessage::m_types = {
    {"A", "Navigational warning"},
    {"B", "Meteorological warning"},
    {"C", "Ice reports"},
    {"D", "Search and rescue"},
    {"E", "Meteorological forecasts"},
    {"F", "Pilot service messages"},
    {"G", "AIS"},
    {"H", "LORAN"},
    {"J", "SATNAV"},
    {"K", "Navaid messages"},
    {"L", "Navigational warning"},
    {"T", "Test transmissions"},
    {"X", "Special services"},
    {"Y", "Special services"},
    {"Z", "No message"}
};

const NavtexTransmitter* NavtexTransmitter::getTransmitter(QTime time, int area, qint64 frequency)
{
    for (const auto& transmitter : NavtexTransmitter::m_navtexTransmitters)
    {
        if (transmitter.m_area == area)
        {
            for (const auto& schedule : transmitter.m_schedules)
            {
                if (schedule.m_frequency == frequency)
                {
                    for (const auto& txStartTime : schedule.m_times)
                    {
                        // Transmitters have 10 minute windows for transmission
                        int secs = txStartTime.secsTo(time);
                        if ((secs >= 0) && (secs < 10*60)) {
                            return &transmitter;
                        }
                    }
                }
            }
        }
    }
    return nullptr;
}

NavtexMessage::NavtexMessage(QDateTime dateTime, const QString& stationId, const QString& typeId, const QString& id, const QString& message) :
    m_stationId(stationId),
    m_typeId(typeId),
    m_id(id),
    m_message(message),
    m_dateTime(dateTime),
    m_valid(true)
{
}

NavtexMessage::NavtexMessage(const QString& text)
{
    m_dateTime = QDateTime::currentDateTime();
    QRegularExpression re("[Z*][C*][Z*][C*][ *]([A-Z])([A-Z])(\\d\\d)((.|\n|\r)*)[N*][N*][N*][N*]");

    QRegularExpressionMatch match = re.match(text);
    if (match.hasMatch())
    {
        m_stationId = match.captured(1);
        m_typeId = match.captured(2);
        m_id = match.captured(3);
        m_message = match.captured(4).trimmed();
        m_valid = true;
    }
    else
    {
        m_message = text;
        m_valid = false;
    }
}

QString NavtexMessage::getStation(int area, qint64 frequency) const
{
    for (const auto& transmitter : NavtexTransmitter::m_navtexTransmitters)
    {
        if (transmitter.m_area == area)
        {
            for (const auto& schedule : transmitter.m_schedules)
            {
                if ((schedule.m_id == m_stationId) && (schedule.m_frequency == frequency)) {
                    return transmitter.m_station;
                }
            }
        }

    }
    return "";
}

QString NavtexMessage::getType() const
{
    if (m_valid && m_types.contains(m_typeId)) {
        return m_types.value(m_typeId);
    }
    return "";
}

void SitorBDecoder::init()
{
    m_state = PHASING;
    m_idx = 0;
    m_figureSet = false;
    m_errors = 0;
}

// In:
//   Received 7-bit CCIR476 sequence
// Returns:
//   Decoded ASCII character
//   ETX end of text
//   '*' both chars invalid
//   -1 no character available yet
signed char SitorBDecoder::decode(signed char c)
{
    signed char ret = -1;

    //qDebug() << "In: " << printable(ccir476Decode(c));

    switch (m_state)
    {
    case PHASING:
        // Wait until we get a valid non-phasing character
        if ((c != PHASING_1) && (c != PHASING_2) && (ccir476Decode(c) != -1))
        {
            m_buf[m_idx++] = c;
            m_state = FILL_RX;
        }
        break;

    case FILL_DX:
        // Fill up buffer
        m_buf[m_idx++] = c;
        if (m_idx == BUFFER_SIZE)
        {
            m_state = RX;
            m_idx = 0;
        }
        else
        {
            m_state = FILL_RX;
        }
        break;

    case FILL_RX:
        // Should be phasing 1
        if (c != PHASING_1) {
            m_errors++;
        }
        m_state = FILL_DX;
        break;

    case RX:
        {
            // Try to decode a character
            signed char dx = ccir476Decode(m_buf[m_idx]);
            signed char rx = ccir476Decode(c);
            signed char a;

            // Idle alpha (phasing 1) in both dx and rx means end of signal
            if ((dx == '<') && (rx == '<'))
            {
                a = 0x2; // ETX - End of text
            }
            else if (dx != -1)
            {
                a = dx;  // First received character has no detectable error
                if ((dx != rx) && !((dx == '<') && (rx == '>')) && !((dx == '>') && (rx == '<'))) {
                    m_errors++;
                }
            }
            else if (rx != -1)
            {
                a = rx;  // Second received character has no detectable error
                m_errors++;
            }
            else
            {
                a = '*'; // Both received characters have errors
                m_errors += 2;
            }
            if (a == 0xf) {
                m_figureSet = false;
            } else if (a == 0xe) {
                m_figureSet = true;
            } else {
                ret = a;
            }
            m_state = DX;
        }
        break;

    case DX:
        // Save received character in buffer
        m_buf[m_idx] = c;
        m_idx = (m_idx + 1) % BUFFER_SIZE;
        m_state = RX;
        break;
    }

    return ret;
}

QString SitorBDecoder::printable(signed char c)
{
    if (c == -1) {
        return "Unknown";
    } else if (c == 0x2) {
        return "End of transmission";
    } else if (c == 0xf) {
        return "Letter";
    } else if (c == 0xe) {
        return "Figure";
    } else if (c == 0x5) {
        return "Cross";
    } else if (c == 0x7) {
        return "Bell";
    } else {
        return QString("%1").arg((char)c);
    }
}

// https://www.itu.int/dms_pubrec/itu-r/rec/m/R-REC-M.476-5-199510-I!!PDF-E.pdf - Table 1

// https://www.itu.int/dms_pubrec/itu-r/rec/m/R-REC-M.625-4-201203-I!!PDF-E.pdf

const signed char SitorBDecoder::m_ccir476LetterSetDecode[128] = {
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    0x0d,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    'T',
    -1,
    -1,
    -1,
    0x0a,
    -1,
    ' ',
    'V',
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    'B',
    -1,
    -1,
    -1,
    -1,
    -1,
    0x0f,
    'X',
    -1,
    -1,
    -1,
    -1,
    '>',
    -1,
    'E',
    0x0e,
    -1,
    -1,
    'U',
    'Q',
    -1,
    'K',
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    'O',
    -1,
    -1,
    -1,
    'H',
    -1,
    'N',
    'M',
    -1,
    -1,
    -1,
    -1,
    'L',
    -1,
    'R',
    'G',
    -1,
    -1,
    'I',
    'P',
    -1,
    'C',
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    'Z',
    -1,
    'D',
    -1,
    -1,
    -1,
    'S',
    'Y',
    -1,
    'F',
    -1,
    -1,
    -1,
    -1,
    'A',
    'W',
    -1,
    'J',
    -1,
    -1,
    -1,
    '<',
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
};

const signed char SitorBDecoder::m_ccir476FigureSetDecode[128] = {
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    0x0d,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    '5',
    -1,
    -1,
    -1,
    0x0a,
    -1,
    ' ',
    '=',
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    '?',
    -1,
    -1,
    -1,
    -1,
    -1,
    0x0f,
    '/',
    -1,
    -1,
    -1,
    -1,
    '>',
    -1,
    '3',
    0x0e,
    -1,
    -1,
    '7',
    '1',
    -1,
    '(',
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    '9',
    -1,
    -1,
    -1,
    -93,
    -1,
    ',',
    '.',
    -1,
    -1,
    -1,
    -1,
    ')',
    -1,
    '4',
    '&',
    -1,
    -1,
    '8',
    '0',
    -1,
    ':',
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    '+',
    -1,
    0x05,
    -1,
    -1,
    -1,
    '\'',
    '6',
    -1,
    '!',
    -1,
    -1,
    -1,
    -1,
    '-',
    '2',
    -1,
    0x07,
    -1,
    -1,
    -1,
    '<',
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
};

signed char SitorBDecoder::ccir476Decode(signed char c)
{
    if (m_figureSet) {
        return m_ccir476FigureSetDecode[(int)c];
    } else {
        return m_ccir476LetterSetDecode[(int)c];
    }
}

