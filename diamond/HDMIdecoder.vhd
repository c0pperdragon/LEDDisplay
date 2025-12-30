library ieee;
library machxo2;
use ieee.numeric_std.all;
use ieee.std_logic_1164.all;
use machxo2.all;

entity HDMIDecoder is	
	port (
		C          : in std_logic;
		D0         : in std_logic;		
		D1         : in std_logic;
		D2         : in std_logic;
		
		R_BIT      : out std_logic_vector(1 downto 0);
		G_BIT      : out std_logic_vector(1 downto 0);
		B_BIT      : out std_logic_vector(1 downto 0);
		CLK_BIT    : out std_logic;
		
		SCL        : in std_logic;
		SDA        : inout std_logic;
		
		DUMMY      : out std_logic
	);	

	ATTRIBUTE IO_TYPES : string;
	ATTRIBUTE IO_TYPES OF C:  SIGNAL IS "LVDS,-";
	ATTRIBUTE IO_TYPES OF D0: SIGNAL IS "LVDS,-";
	ATTRIBUTE IO_TYPES OF D1: SIGNAL IS "LVDS,-";
	ATTRIBUTE IO_TYPES OF D2: SIGNAL IS "LVDS,-";
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
	PORT MAP ( CLKI => C, CLKOP => CLK, CLKOS => CLKFAST );
	
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
			r := r(8 downto 0) & D0;
			g := g(8 downto 0) & D1;
			b := b(8 downto 0) & D2;
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
	variable x : integer range 0 to 1024 := 0;
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
			if x<320 then
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
			else
				if x<340 and phase<=1 then
					CLK_BIT <= '1';
				else
					CLK_BIT <= '0';
				end if;
				if y=0 then
					R_BIT <= "00";
					G_BIT <= "00";
					B_BIT <= "00";
				else
					R_BIT <= "11";
					G_BIT <= "11";
					B_BIT <= "11";
				end if;
			end if;
			-- progress counters
			if phase<3 then
				phase:=phase+1;
			else
				phase := 0;
				if x<530-1 then
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

    -- send EDID
	process (SCL,SDA)
	type Tedid is array (0 to 255) of integer;
	constant edid:Tedid := 
	(  	16#00#,16#ff#,16#ff#,16#ff#,16#ff#,16#ff#,16#ff#,16#00#,                                                                    -- standard header
		16#0d#,16#f0#,16#01#,16#00#,16#01#,16#00#,16#00#,16#00#,16#0a#,16#22#,                                                      -- manufacturer info
		16#01#,16#03#,                                                                                                                -- EDID version
		16#a0#,                                                                                                                       -- video input parameters
		16#28#,16#1e#,                                                                                                                -- screen size in cm
		16#78#,                                                                                                                       -- gamma factor
		16#06#,                                                                                                                       -- supported features bitmap    
		16#EE#,16#91#,16#A3#,16#54#,16#4C#,16#99#,16#26#,16#0F#,16#50#,16#54#,                                                      -- chromaticity coordinates 
		16#00#,16#00#,16#00#,                                                                                                         -- established modes
		16#01#,16#01#,16#01#,16#01#,16#01#,16#01#,16#01#,16#01#,16#01#,16#01#,16#01#,16#01#,16#01#,16#01#,16#01#,16#01#,           -- standard modes
		16#8c#,16#0a#,16#a0#,16#20#,16#51#,16#20#,16#18#,16#10#,16#18#,16#80#,16#33#,16#00#,16#90#,16#2c#,16#11#,16#00#,16#00#,16#18#, -- detailed timing descriptor: 14416#288
		16#00#,16#00#,16#00#,16#fc#,16#00#,16#48#,16#44#,16#4d#,16#49#,16#32#,16#53#,16#43#,16#41#,16#52#,16#54#,16#0a#,16#20#,16#20#, -- monitor information
		16#00#,16#00#,16#00#,16#10#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#, -- dummy
		16#00#,16#00#,16#00#,16#10#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#, -- dummy
		16#01#,                                                                                                                           -- number of extension blocks
		16#00#,                                                                                                                           -- place for checksum
		16#02#,16#03#,16#10#,16#40#,                                                                                                     -- header: support basic audio, 0 native formats
		16#23#,16#09#,16#07#,16#07#,                                                                                                     -- audio data block
		16#83#,16#01#,16#20#,16#20#,                                                                                                     -- speaker allocation
		16#63#,16#03#,16#0c#,16#00#,                                                                                                     -- IEEE registration identifier
		16#00#,16#00#,16#00#,16#00#,
		16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,
		16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,
		16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,
		16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,
		16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,
		16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#,16#00#  
	);
	type Tstate is (idle, address, ack, transfer);
	variable state: Tstate := idle;
	variable scl_on_falling_sda:std_logic := '0';
	variable sda_on_rising_scl:std_logic := '0';
	variable writebuffer:std_logic_vector(6 downto 0);
	variable readbuffer:std_logic_vector(7 downto 0);
	variable bitcount:integer range 7 downto 0;
	variable readmode:boolean;
	variable index:integer range 0 to 255 := 0;
	variable isfirstbyte:boolean;
	variable out_sda:std_logic := '1';
	begin
		if falling_edge(SDA) then
			scl_on_falling_sda := SDA;
		end if;
		if rising_edge(SCL) then
			sda_on_rising_scl := SCL;
		end if;
		if falling_edge(SCL) then
			SDA <= 'Z';
			-- detect a previous start condition
			if SDA='0' and scl_on_falling_sda='1' then   
				state := address;
				bitcount := 0;
			else
				-- state machine
				case state is
				when address =>
					if bitcount<7 then
						writebuffer := writebuffer(5 downto 0) & sda_on_rising_scl;
						bitcount := bitcount+1;
					elsif writebuffer = "1010000" then
						readmode := sda_on_rising_scl='1';
						state := ack;
						SDA <= '0';   -- ack after matching address
					else
						state := idle;
					end if;
				when ack =>
					state := transfer;
					isfirstbyte := true;
					bitcount := 0;
					if readmode then
						readbuffer := std_logic_vector(to_unsigned(edid(index), 8));
						SDA <= readbuffer(7);
						readbuffer := readbuffer(6 downto 0) & '0';
					end if;
				when transfer =>
					if bitcount<7 then
						writebuffer := writebuffer(5 downto 0) & sda_on_rising_scl;
						bitcount := bitcount+1;
						if readmode then
							SDA <= readbuffer(7);
							readbuffer := readbuffer(6 downto 0) & '0';
						end if;
					else
						if isfirstbyte and not readmode then
							index := to_integer(signed(writebuffer & sda_on_rising_scl));
						else
							index := index+1;
						end if;
						if not readmode then
							SDA <= '0';  -- ack when writing to slave
						end if;
						isfirstbyte := false;
						state := ack;
					end if;				
				when others =>
				end case;			
			end if;
		end if;
	end process;

end immediate;
