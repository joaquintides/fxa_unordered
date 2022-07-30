/* Proof of concept of closed- and open-addressing
 * unordered associative containers.
 *
 * Copyright 2022 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */
 
#ifndef FOA_UNORDERED_UINT64_XOPS_HPP
#define FOA_UNORDERED_UINT64_XOPS_HPP
 
#include <cassert>
#include <cstdint>

namespace fxa_unordered{

namespace uint64_xops{

constexpr inline uint64_t smask(uint64_t n)
{
  return
    (n&  1)<<   0|
    (n&  2)<<   7|
    (n&  4)<<  14|
    (n&  8)<<  21|
    (n& 16)<<  28|
    (n& 32)<<  35|
    (n& 64)<<  42|
    (n&128)<<  49;
}
    
constexpr inline uint64_t simask(uint64_t n)
{
  return smask(~n);
}

static constexpr uint64_t smasks[]=
{
  smask(0),smask(1),smask(2),smask(3),smask(4),smask(5),smask(6),smask(7),
  smask(8),smask(9),smask(10),smask(11),smask(12),smask(13),smask(14),smask(15),
  smask(16),smask(17),smask(18),smask(19),smask(20),smask(21),smask(22),smask(23),
  smask(24),smask(25),smask(26),smask(27),smask(28),smask(29),smask(30),smask(31),
  smask(32),smask(33),smask(34),smask(35),smask(36),smask(37),smask(38),smask(39),
  smask(40),smask(41),smask(42),smask(43),smask(44),smask(45),smask(46),smask(47),
  smask(48),smask(49),smask(50),smask(51),smask(52),smask(53),smask(54),smask(55),
  smask(56),smask(57),smask(58),smask(59),smask(60),smask(61),smask(62),smask(63),
  smask(64),smask(65),smask(66),smask(67),smask(68),smask(69),smask(70),smask(71),
  smask(72),smask(73),smask(74),smask(75),smask(76),smask(77),smask(78),smask(79),
  smask(80),smask(81),smask(82),smask(83),smask(84),smask(85),smask(86),smask(87),
  smask(88),smask(89),smask(90),smask(91),smask(92),smask(93),smask(94),smask(95),
  smask(96),smask(97),smask(98),smask(99),smask(100),smask(101),smask(102),smask(103),
  smask(104),smask(105),smask(106),smask(107),smask(108),smask(109),smask(110),smask(111),
  smask(112),smask(113),smask(114),smask(115),smask(116),smask(117),smask(118),smask(119),
  smask(120),smask(121),smask(122),smask(123),smask(124),smask(125),smask(126),smask(127),
  smask(128),smask(129),smask(130),smask(131),smask(132),smask(133),smask(134),smask(135),
  smask(136),smask(137),smask(138),smask(139),smask(140),smask(141),smask(142),smask(143),
  smask(144),smask(145),smask(146),smask(147),smask(148),smask(149),smask(150),smask(151),
  smask(152),smask(153),smask(154),smask(155),smask(156),smask(157),smask(158),smask(159),
  smask(160),smask(161),smask(162),smask(163),smask(164),smask(165),smask(166),smask(167),
  smask(168),smask(169),smask(170),smask(171),smask(172),smask(173),smask(174),smask(175),
  smask(176),smask(177),smask(178),smask(179),smask(180),smask(181),smask(182),smask(183),
  smask(184),smask(185),smask(186),smask(187),smask(188),smask(189),smask(190),smask(191),
  smask(192),smask(193),smask(194),smask(195),smask(196),smask(197),smask(198),smask(199),
  smask(200),smask(201),smask(202),smask(203),smask(204),smask(205),smask(206),smask(207),
  smask(208),smask(209),smask(210),smask(211),smask(212),smask(213),smask(214),smask(215),
  smask(216),smask(217),smask(218),smask(219),smask(220),smask(221),smask(222),smask(223),
  smask(224),smask(225),smask(226),smask(227),smask(228),smask(229),smask(230),smask(231),
  smask(232),smask(233),smask(234),smask(235),smask(236),smask(237),smask(238),smask(239),
  smask(240),smask(241),smask(242),smask(243),smask(244),smask(245),smask(246),smask(247),
  smask(248),smask(249),smask(250),smask(251),smask(252),smask(253),smask(254),smask(255),
};

static constexpr uint64_t simasks[]=
{
  simask(0),simask(1),simask(2),simask(3),simask(4),simask(5),simask(6),simask(7),
  simask(8),simask(9),simask(10),simask(11),simask(12),simask(13),simask(14),simask(15),
  simask(16),simask(17),simask(18),simask(19),simask(20),simask(21),simask(22),simask(23),
  simask(24),simask(25),simask(26),simask(27),simask(28),simask(29),simask(30),simask(31),
  simask(32),simask(33),simask(34),simask(35),simask(36),simask(37),simask(38),simask(39),
  simask(40),simask(41),simask(42),simask(43),simask(44),simask(45),simask(46),simask(47),
  simask(48),simask(49),simask(50),simask(51),simask(52),simask(53),simask(54),simask(55),
  simask(56),simask(57),simask(58),simask(59),simask(60),simask(61),simask(62),simask(63),
  simask(64),simask(65),simask(66),simask(67),simask(68),simask(69),simask(70),simask(71),
  simask(72),simask(73),simask(74),simask(75),simask(76),simask(77),simask(78),simask(79),
  simask(80),simask(81),simask(82),simask(83),simask(84),simask(85),simask(86),simask(87),
  simask(88),simask(89),simask(90),simask(91),simask(92),simask(93),simask(94),simask(95),
  simask(96),simask(97),simask(98),simask(99),simask(100),simask(101),simask(102),simask(103),
  simask(104),simask(105),simask(106),simask(107),simask(108),simask(109),simask(110),simask(111),
  simask(112),simask(113),simask(114),simask(115),simask(116),simask(117),simask(118),simask(119),
  simask(120),simask(121),simask(122),simask(123),simask(124),simask(125),simask(126),simask(127),
  simask(128),simask(129),simask(130),simask(131),simask(132),simask(133),simask(134),simask(135),
  simask(136),simask(137),simask(138),simask(139),simask(140),simask(141),simask(142),simask(143),
  simask(144),simask(145),simask(146),simask(147),simask(148),simask(149),simask(150),simask(151),
  simask(152),simask(153),simask(154),simask(155),simask(156),simask(157),simask(158),simask(159),
  simask(160),simask(161),simask(162),simask(163),simask(164),simask(165),simask(166),simask(167),
  simask(168),simask(169),simask(170),simask(171),simask(172),simask(173),simask(174),simask(175),
  simask(176),simask(177),simask(178),simask(179),simask(180),simask(181),simask(182),simask(183),
  simask(184),simask(185),simask(186),simask(187),simask(188),simask(189),simask(190),simask(191),
  simask(192),simask(193),simask(194),simask(195),simask(196),simask(197),simask(198),simask(199),
  simask(200),simask(201),simask(202),simask(203),simask(204),simask(205),simask(206),simask(207),
  simask(208),simask(209),simask(210),simask(211),simask(212),simask(213),simask(214),simask(215),
  simask(216),simask(217),simask(218),simask(219),simask(220),simask(221),simask(222),simask(223),
  simask(224),simask(225),simask(226),simask(227),simask(228),simask(229),simask(230),simask(231),
  simask(232),simask(233),simask(234),simask(235),simask(236),simask(237),simask(238),simask(239),
  simask(240),simask(241),simask(242),simask(243),simask(244),simask(245),simask(246),simask(247),
  simask(248),simask(249),simask(250),simask(251),simask(252),simask(253),simask(254),simask(255),
};

constexpr void set(uint64_t& x,unsigned pos,unsigned n)
{
  assert(n<256&&pos<8);
    
  x|=   smasks[n]<<pos;
  x&=~(simasks[n]<<pos);
}

constexpr inline uint64_t mmask(uint64_t n)
{
  uint64_t m=0;
  for(unsigned pos=0;pos<8;++pos)set(m,pos,n);
  return m;
}

static constexpr uint64_t mmasks[]=
{
  mmask(0),mmask(1),mmask(2),mmask(3),mmask(4),mmask(5),mmask(6),mmask(7),
  mmask(8),mmask(9),mmask(10),mmask(11),mmask(12),mmask(13),mmask(14),mmask(15),
  mmask(16),mmask(17),mmask(18),mmask(19),mmask(20),mmask(21),mmask(22),mmask(23),
  mmask(24),mmask(25),mmask(26),mmask(27),mmask(28),mmask(29),mmask(30),mmask(31),
  mmask(32),mmask(33),mmask(34),mmask(35),mmask(36),mmask(37),mmask(38),mmask(39),
  mmask(40),mmask(41),mmask(42),mmask(43),mmask(44),mmask(45),mmask(46),mmask(47),
  mmask(48),mmask(49),mmask(50),mmask(51),mmask(52),mmask(53),mmask(54),mmask(55),
  mmask(56),mmask(57),mmask(58),mmask(59),mmask(60),mmask(61),mmask(62),mmask(63),
  mmask(64),mmask(65),mmask(66),mmask(67),mmask(68),mmask(69),mmask(70),mmask(71),
  mmask(72),mmask(73),mmask(74),mmask(75),mmask(76),mmask(77),mmask(78),mmask(79),
  mmask(80),mmask(81),mmask(82),mmask(83),mmask(84),mmask(85),mmask(86),mmask(87),
  mmask(88),mmask(89),mmask(90),mmask(91),mmask(92),mmask(93),mmask(94),mmask(95),
  mmask(96),mmask(97),mmask(98),mmask(99),mmask(100),mmask(101),mmask(102),mmask(103),
  mmask(104),mmask(105),mmask(106),mmask(107),mmask(108),mmask(109),mmask(110),mmask(111),
  mmask(112),mmask(113),mmask(114),mmask(115),mmask(116),mmask(117),mmask(118),mmask(119),
  mmask(120),mmask(121),mmask(122),mmask(123),mmask(124),mmask(125),mmask(126),mmask(127),
  mmask(128),mmask(129),mmask(130),mmask(131),mmask(132),mmask(133),mmask(134),mmask(135),
  mmask(136),mmask(137),mmask(138),mmask(139),mmask(140),mmask(141),mmask(142),mmask(143),
  mmask(144),mmask(145),mmask(146),mmask(147),mmask(148),mmask(149),mmask(150),mmask(151),
  mmask(152),mmask(153),mmask(154),mmask(155),mmask(156),mmask(157),mmask(158),mmask(159),
  mmask(160),mmask(161),mmask(162),mmask(163),mmask(164),mmask(165),mmask(166),mmask(167),
  mmask(168),mmask(169),mmask(170),mmask(171),mmask(172),mmask(173),mmask(174),mmask(175),
  mmask(176),mmask(177),mmask(178),mmask(179),mmask(180),mmask(181),mmask(182),mmask(183),
  mmask(184),mmask(185),mmask(186),mmask(187),mmask(188),mmask(189),mmask(190),mmask(191),
  mmask(192),mmask(193),mmask(194),mmask(195),mmask(196),mmask(197),mmask(198),mmask(199),
  mmask(200),mmask(201),mmask(202),mmask(203),mmask(204),mmask(205),mmask(206),mmask(207),
  mmask(208),mmask(209),mmask(210),mmask(211),mmask(212),mmask(213),mmask(214),mmask(215),
  mmask(216),mmask(217),mmask(218),mmask(219),mmask(220),mmask(221),mmask(222),mmask(223),
  mmask(224),mmask(225),mmask(226),mmask(227),mmask(228),mmask(229),mmask(230),mmask(231),
  mmask(232),mmask(233),mmask(234),mmask(235),mmask(236),mmask(237),mmask(238),mmask(239),
  mmask(240),mmask(241),mmask(242),mmask(243),mmask(244),mmask(245),mmask(246),mmask(247),
  mmask(248),mmask(249),mmask(250),mmask(251),mmask(252),mmask(253),mmask(254),mmask(255),
};

int match(uint64_t x,int n)
{
  assert(n<256);
  
  x=~(x^mmasks[n]);
  x&=x>>32;
  x&=x>>16;
  x&=x>>8;
  return x;
}

} // namespace uint64_xops

} // namespace fxa_unordered

#endif
