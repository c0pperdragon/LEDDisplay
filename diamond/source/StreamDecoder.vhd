library ieee;
library machxo2;
use ieee.numeric_std.all;
use ieee.std_logic_1164.all;
use machxo2.all;

entity StreamDecoder is	
	port (
		PIXELCLOCK : in std_logic;
		BITCLOCK0  : in std_logic;
		BITCLOCK1  : in std_logic;
		BITCLOCK2  : in std_logic;
		BITCLOCK3  : in std_logic;
		
		STREAM     : in std_logic;  
		
		DE         : out std_logic;
		DATA       : out std_logic_vector(3 downto 0);
		SYNCED     : out std_logic
	);	
end entity;

architecture immediate of StreamDecoder is
begin

	process (BITCLOCK0,BITCLOCK1,PIXELCLOCK)	
	-- phase shift of incomming 10 bits in relation to PIXELCLOCK (higher value means: later)
	variable phase:integer range 0 to 79 := 0;
	-- parallel input stream with different start phase and same end clock
	variable a:std_logic_vector(8 downto 0);
	variable b:std_logic_vector(8 downto 0);
	variable c:std_logic_vector(8 downto 0);
	variable d:std_logic_vector(8 downto 0);
	variable e:std_logic_vector(8 downto 0);
	variable f:std_logic_vector(8 downto 0);
	variable g:std_logic_vector(8 downto 0);
	variable h:std_logic_vector(8 downto 0);
	variable selectedqueue:integer range 0 to 7; -- clocked by BITCLOCK0
	variable x:std_logic_vector(18 downto 0);    -- clocked by BITCLOCK0
	variable y:std_logic_vector(18 downto 0);    -- clocked by PIXELCLOCK
	variable bits:std_logic_vector(9 downto 0);  -- clocked by PIXELCLOCK
	-- decoding 
	variable de_out:std_logic;
	variable count_testpixel:integer range 0 to 2047;
	variable count_non_de:integer range 0 to 255;
	variable max_non_de:integer range 0 to 255;
	variable progressphase:boolean;
	begin
		if rising_edge(PIXELCLOCK) then
			-- check behaviour of detecting the special symbols - need to tune to correct phase
			-- in any 2000 pixel range, there must be at least one continous stretch of 200 non-de pixels
			progressphase := false;
			if count_testpixel<2000 then
				count_testpixel := count_testpixel+1;
				if de_out='1' then
					count_non_de := 0;
				elsif count_non_de<200 then
					count_non_de := count_non_de+1;
					if count_non_de > max_non_de then
						max_non_de := count_non_de;
					end if;
				end if;
			else
				if max_non_de<200 then
					progressphase := true;
				end if;			
				count_testpixel := 0;
				count_non_de := 0;
				max_non_de := 0;
			end if;

			-- output processing
			if bits="1101010100" or bits="0010101011" or bits="0101010100" or bits="1010101011" then
				de_out := '0';
				DE <= '0';
			else
				de_out := '1';
				DE <= '1';
				if bits(8)='1' then
					DATA(3) <= bits(7) xor bits(6);
					DATA(2) <= bits(6) xor bits(5);
					DATA(1) <= bits(5) xor bits(4);
					DATA(0) <= bits(4) xor bits(3);
				else
					DATA(3) <= not (bits(7) xor bits(6));
					DATA(2) <= not (bits(6) xor bits(5));
					DATA(1) <= not (bits(5) xor bits(4));
					DATA(0) <= not (bits(4) xor bits(3));					
				end if;
			end if;
			-- select bits according to coarse phase
			bits(0) := y(0+phase/8);
			bits(1) := y(1+phase/8);
			bits(2) := y(2+phase/8);
			bits(3) := y(3+phase/8);
			bits(4) := y(4+phase/8);
			bits(5) := y(5+phase/8);
			bits(6) := y(6+phase/8);
			bits(7) := y(7+phase/8);
			bits(8) := y(8+phase/8);
			bits(9) := y(9+phase/8);
			-- take into slow clock domain 
			y := x;
			
			-- try to find better phase for future sampling
			if progressphase then
				if phase<79 then 
					phase := phase+1;
				else
					phase := 0;
				end if;
			end if;
		end if;
		
		-- processing on earliest clock
		if rising_edge(BITCLOCK0) then 
			-- fetch from queue as specified by fine phase			
			x(17 downto 0) := x(18 downto 1);
			case selectedqueue is
			when 0 => x(18) := a(0);
			when 1 => x(18) := b(0);
			when 2 => x(18) := c(0);
			when 3 => x(18) := d(0);
			when 4 => x(18) := e(0);
			when 5 => x(18) := f(0);
			when 6 => x(18) := g(0);
			when others => x(18) := h(0);
			end case;
			-- move bits through various queues at this clock
			a(8 downto 0) := STREAM & a(8 downto 1);
			b(0) := b(1);
			c(0) := c(1);
			d(0) := d(1);
			e(0) := e(1);
			f(0) := f(1);
			g(0) := g(1);
			h(0) := h(1);
			-- take queue selector from slow clock to fast clock
			selectedqueue := phase mod 8;
		end if;
		-- processing on later clocks
		if rising_edge(BITCLOCK1) then
			b(8 downto 1) := STREAM & b(8 downto 2);
			c(1) := c(2);
			d(1) := d(2);
			e(1) := e(2);
			f(1) := f(2);
			g(1) := g(2);
			h(1) := h(2);
		end if;
		if rising_edge(BITCLOCK2) then
			c(8 downto 2) := STREAM & c(8 downto 3);
			d(2) := d(3);
			e(2) := e(3);
			f(2) := f(3);
			g(2) := g(3);
			h(2) := h(3);
		end if;
		if rising_edge(BITCLOCK3) then
			d(8 downto 3) := STREAM & d(8 downto 4);
			e(3) := e(4);
			f(3) := f(4);
			g(3) := g(4);
			h(3) := h(4);
		end if;
		if falling_edge(BITCLOCK0) then
			e(8 downto 4) := STREAM & e(8 downto 5);
			f(4) := f(5);
			g(4) := g(5);
			h(4) := h(5);
		end if;
		if falling_edge(BITCLOCK1) then
			f(8 downto 5) := STREAM & f(8 downto 6);
			g(5) := g(6);
			h(5) := h(6);
		end if;
		if falling_edge(BITCLOCK2) then
			g(8 downto 6) := STREAM & g(8 downto 7);
			h(6) := h(7);
		end if;		
		if falling_edge(BITCLOCK3) then
			h(8 downto 7) := STREAM & h(8 downto 8);
		end if;
		
	end process;
		
end immediate;
