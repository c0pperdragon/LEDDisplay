library ieee;
library machxo2;
use ieee.numeric_std.all;
use ieee.std_logic_1164.all;
use machxo2.all;

entity HDMIDecoder is	
	port (
		CLKIN       : in std_logic;
		RED         : in std_logic;		
		GREEN       : in std_logic;
		BLUE        : in std_logic;
		
		R_BIT       : out std_logic_vector(1 downto 0);
		G_BIT       : out std_logic_vector(1 downto 0);
		B_BIT       : out std_logic_vector(1 downto 0);
		CLK_BIT     : out std_logic;
		
		DUMMY       : out std_logic
	);	

	ATTRIBUTE IO_TYPES : string;
	ATTRIBUTE IO_TYPES OF CLKIN: SIGNAL IS "LVDS,-";
	ATTRIBUTE IO_TYPES OF RED: SIGNAL IS "LVDS,-";
	ATTRIBUTE IO_TYPES OF GREEN: SIGNAL IS "LVDS,-";
	ATTRIBUTE IO_TYPES OF BLUE: SIGNAL IS "LVDS,-";
end entity;

architecture immediate of HDMIDecoder is


-- PLL outputs 2 clocks: 
-- pass-through pixel input clock ()
-- 10x bit clock 
component PLLHDMI is
    port (
        CLKI: in  std_logic;    -- 25.175 MHz
        CLKOP: out  std_logic;  -- 25.175 MHz direct pass-through
        CLKOS: out  std_logic); -- 255.175 MHz bit clock
end component;

COMPONENT OSCH
	GENERIC (NOM_FREQ: string);
	PORT (
		STDBY:IN std_logic;
		OSC:OUT std_logic;
		SEDSTDBY:OUT std_logic
	);
END COMPONENT;


signal CLK:std_logic;
signal CLKFAST:std_logic;

signal TESTCLK:std_logic;  

begin
	pll: PLLHDMI
	PORT MAP ( CLKI => CLKIN, CLKOP => CLK, CLKOS => CLKFAST );
	
	OSCInst0: OSCH
	GENERIC MAP( NOM_FREQ => "26.6" )
	PORT MAP ( 
		STDBY=> '0', OSC => TESTCLK,	SEDSTDBY => open 
	);

	process (CLK,CLKFAST)	
	variable r:std_logic_vector(9 downto 0);
	variable g:std_logic_vector(9 downto 0);
	variable b:std_logic_vector(9 downto 0);
	variable bits:std_logic_vector(29 downto 0);
	begin
		if rising_edge(CLKFAST) then
			r := r(8 downto 0) & RED;
			g := g(8 downto 0) & GREEN;
			b := b(8 downto 0) & BLUE;
		end if;
		if rising_edge(CLK) then
			DUMMY <= bits(29) xor bits(28) xor bits(27) xor bits(26) xor bits(25) xor bits(24) xor bits(23) xor bits(22) xor bits(21) xor bits(20)
			    xor bits(19) xor bits(18) xor bits(17) xor bits(16) xor bits(15) xor bits(14) xor bits(13) xor bits(12) xor bits(11) xor bits(10)
				xor bits(9) xor bits(8) xor bits(7) xor bits(6) xor bits(5) xor bits(4) xor bits(3) xor bits(2) xor bits(1) xor bits(0);
			bits := r & g & b;
		end if;
	end process;
	
	
	process (TESTCLK)
	variable phase : integer range 0 to 3 := 0;
	variable x : integer range 0 to 511 := 0;
	variable y : integer range 0 to 255 := 0;
	variable frame : integer range 0 to 255;
	variable rgb:std_logic_vector(11 downto 0);
	begin
		if rising_edge(TESTCLK) then
			-- generate picture
			rgb := "000000000000";
			if x<320 and y<256 then
				if x<256 then
					rgb(11 downto 8) := std_logic_vector(to_unsigned(x/16, 4));
					rgb(7 downto 4)  := std_logic_vector(to_unsigned(y/16, 4));
				else
					rgb(11 downto 8) := std_logic_vector(to_unsigned( (x+frame)mod 16, 4));
					rgb(7 downto 4)  := std_logic_vector(to_unsigned( (x+frame)mod 16, 4));
					rgb(3 downto 0)  := std_logic_vector(to_unsigned( (x+frame)mod 16, 4));
				end if;
			end if;	
			-- generate output signals
			if x>=320 then
				CLK_BIT <= '0';
				if y=0 then
					R_BIT <= "00";
					G_BIT <= "00";
					B_BIT <= "00";
				else
					R_BIT <= "11";
					G_BIT <= "11";
					B_BIT <= "11";
				end if;
			else
				if phase<=1 then
					CLK_BIT <= '1';
					R_BIT <= rgb(11 downto 10);
					G_BIT <= rgb(7 downto 6);
					B_BIT <= rgb(3 downto 2);
				else 
					CLK_BIT <= '0';
					R_BIT <= rgb(9 downto 8);
					G_BIT <= rgb(5 downto 4);
					B_BIT <= rgb(1 downto 0);
				end if;
			end if;
			-- progress counters
			if phase<3 then
				phase:=phase+1;
			else
				phase := 0;
				if x<433-1 then
					x := x+1;
				else 
					x := 0;
					if y<256-1 then
						y := y+1;
					else 
						y := 0;
						frame := (frame+1) mod 256;
					end if;
				end if;
			end if;
		end if;
	end process;

end immediate;
