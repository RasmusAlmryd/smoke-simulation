#include "perf.h"

#include <SDL.h>

#include <imgui.h>

#include <unordered_map>
#include <vector>

#include <sstream>
#include <fstream>
#include <algorithm>

#include <any>

#include <GL/glew.h>

#include "labhelper.h"

#define LOG_FATAL(s) fatal_error(s)
//#define CHAG_USE_CUDA

namespace labhelper
{
namespace perf
{
using duration_t = std::chrono::duration<int64_t, std::chrono::nanoseconds::period>;
using timestamp_t = std::chrono::time_point<std::chrono::high_resolution_clock, duration_t>;

namespace
{
struct time_event_durations_t
{
	duration_t cpu;
	duration_t gl;
	duration_t cuda;

	time_event_durations_t operator+( const time_event_durations_t& b )
	{
		time_event_durations_t r = *this;
		r.cpu += b.cpu;
		r.gl += b.gl;
		r.cuda += b.cuda;
		return r;
	}

	time_event_durations_t operator*( double f )
	{
		time_event_durations_t r = *this;
		r.cpu = std::chrono::duration_cast<duration_t>(r.cpu * f);
		r.gl = std::chrono::duration_cast<duration_t>(r.gl * f);
		r.cuda = std::chrono::duration_cast<duration_t>(r.cuda * f);
		return r;
	}
};

struct time_event_t
{
	std::string name;
	timestamp_t start;
	time_event_durations_t duration;

	std::any cpu_data;
	std::any gl_data;
	std::any cuda_data;

	std::vector<time_event_t> children;
};

std::vector<time_event_t> events;

std::vector<time_event_t> event_stack;

std::unordered_map<std::string, time_event_durations_t> time_running_avg;
std::unordered_map<std::string, time_event_durations_t> time_running_avg_tmp;

float seconds_to_record = 2;
duration_t remaining_recording_seconds = {};
std::unordered_map<std::string, std::vector<time_event_durations_t>> time_recordings;

timestamp_t last_frame_time = {};


timestamp_t getTimestamp() { return std::chrono::high_resolution_clock::now(); }

}   // namespace

namespace cpu
{
void start_timer( time_event_t& e );

void stop_timer( time_event_t& e );

void sync();
}   // namespace cpu

namespace gl
{
void start_timer( time_event_t& e );

void stop_timer( time_event_t& e );

void sync();
}   // namespace gl

namespace cuda
{
void start_timer( time_event_t& e );

void stop_timer( time_event_t& e );

void sync();
}   // namespace cuda


void pushTimer( const ::std::string& str )
{
	event_stack.push_back( time_event_t{} );

	event_stack.back().name = str;
	event_stack.back().start = getTimestamp();

	cpu::start_timer( event_stack.back() );
	gl::start_timer( event_stack.back() );
	cuda::start_timer( event_stack.back() );
}

void popTimer()
{
	if ( event_stack.empty() )
	{
		LOG_FATAL( "Trying to pop empty event stack" );
		return;
	}

	cuda::stop_timer( event_stack.back() );
	gl::stop_timer( event_stack.back() );
	cpu::stop_timer( event_stack.back() );

	if ( event_stack.size() > 1 )
	{
		event_stack[event_stack.size() - 2].children.push_back( std::move( event_stack[event_stack.size() - 1] ) );
	}
	else if ( event_stack.size() == 1 )
	{
		events.push_back( std::move( event_stack[0] ) );
	}
	event_stack.pop_back();
}

Scope::Scope( const std::string& name )
{
	pushTimer( name );
}

Scope::~Scope()
{
	popTimer();
}

namespace
{
struct
{
	double running_avg_mult = 0.98;

	float yellow_start = 0.3f;
	float yellow_end = 4.f;
	float orange_start = 10.f;
	float orange_end = 28.f;
} settings;

void draw_time_column( duration_t d )
{
	ImGui::TableNextColumn();
	float s = d.count() / 1'000'000.f;
	ImVec4 c( 1, 1, 1, 1 );
	c.z = std::min( 1.f, std::max( 0.f, 1.f - (s - settings.yellow_start) / (settings.yellow_end - settings.yellow_start) ) );
	c.y = std::min( 1.f, std::max( 0.f, 1.f - (s - settings.orange_start) / (settings.orange_end - settings.orange_start) ) );
	ImGui::TextColored( c, "% 10.5f ms", s );
}

void draw_events( time_event_t& e, const std::string& path )
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen;
	if ( e.children.empty() )
	{
		flags = flags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;
	}

	time_event_durations_t avg;
	auto it = time_running_avg.find( path );
	if ( it != time_running_avg.end() )
	{
		avg = it->second;
	}
	else
	{
		avg = e.duration;
	}
	avg = avg * settings.running_avg_mult + e.duration * (1 - settings.running_avg_mult);
	time_running_avg_tmp[path] = avg;
	e.duration = avg;

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	bool open = ImGui::TreeNodeEx( e.name.c_str(), flags );

	draw_time_column( avg.cpu );

	draw_time_column( avg.gl );

#ifdef CHAG_USE_CUDA
	draw_time_column( avg.cuda );
#endif

	if ( open )
	{
		std::sort( e.children.begin(), e.children.end(), []( const time_event_t& a, const time_event_t& b ) -> bool {
			return a.start < b.start;
		} );

		for ( auto& c : e.children )
		{
			draw_events( c, path + "~" + c.name );
		}

		ImGui::TreePop();
	}
}

#if USE_FMT
std::string printf_events()
{
	const size_t indent = 2;
	size_t max_len = 5;
	const auto count_name_size = [&]( const time_event_t& e ) -> void
	{
		auto count_impl = [&]( const time_event_t& e, size_t depth, auto& count_ref ) mutable -> void {
			if ( max_len < e.name.size() + indent * depth )
			{
				max_len = e.name.size() + indent * depth;
			}
			for ( const auto& c : e.children )
			{
				count_ref( c, depth + 1, count_ref );
			}
		};
		count_impl( e, 0, count_impl );
	};

	for ( const auto& e : events )
	{
		count_name_size( e );
	}

	max_len += 2;
	const auto stringify = [&]( const time_event_t& e ) -> std::string
	{
		auto stringify_impl = [&]( const time_event_t& e, std::string depth, auto& stringify_ref ) mutable -> std::string {
			std::string s;
			std::string deepth = depth;
			s += depth + e.name;
			s.resize( max_len, '.' );
			s += fmt::format( "{: 10.5f} ms    {: 10.5f} ms    {: 10.5f} ms\n",
							  e.duration.cpu.count() / 1'000'000.f,
							  e.duration.gl.count() / 1'000'000.f,
							  e.duration.cuda.count() / 1'000'000.f );
			deepth.resize( deepth.size() + indent, ' ' );

			for ( const auto& c : e.children )
			{
				s += stringify_ref( c, deepth, stringify_ref );
			}
			return s;
		};
		return stringify_impl( e, "", stringify_impl );
	};

	std::string s;
	s = "Event";
	s.resize( max_len + 2, ' ' );
	s += fmt::format( " {:<17}{:<17}{}\n", "CPU", "OpenGL", "CUDA" );
	for ( const auto& e : events )
	{
		s += stringify( e );
	}

	return s;
}
#endif

void record_events()
{
	const auto record_rec = [&]( const time_event_t& e )
	{
		auto record_rec_impl = [&]( const time_event_t& e, const std::string& event_path, auto& record_rec_ref ) mutable -> void {
			std::string current_event_path = event_path + "/" + e.name;
			time_recordings[current_event_path].push_back( e.duration );
			for ( const auto& c : e.children )
			{
				record_rec_ref( c, current_event_path, record_rec_ref );
			}
		};

		record_rec_impl( e, "", record_rec_impl );
	};

	for ( const auto& e : events )
	{
		record_rec( e );
	}
}

}   // namespace


void drawEventsWindow()
{
	if ( event_stack.size() == 1 && event_stack[0].name == "Frame" )
	{
		popTimer();
	}
	if ( !event_stack.empty() )
	{
		LOG_FATAL( " Unbalanced pushTimer/popTimer!" );
	}
	pushTimer( "Frame" );

	cuda::sync();
	gl::sync();
	cpu::sync();

	std::sort( events.begin(), events.end(), []( const time_event_t& a, const time_event_t& b ) -> bool {
		return a.start < b.start;
	} );

	ImGui::Begin( "Performance Timings" );
	{
#if USE_FMT
		bool copy_text = false;
		if ( ImGui::Button( "Copy Text" ) )
		{
			copy_text = true;
		}

		ImGui::SameLine();
#endif

#if RECORD_TIMINGS
		ImGui::SetNextItemWidth( 80 );
		ImGui::InputFloat( "seconds##Seconds to record", &seconds_to_record );

		ImGui::SameLine();

		if ( ImGui::Button( "Record Timings" ) )
		{
			remaining_recording_seconds = std::chrono::duration_cast<duration_t>(
				std::chrono::duration<float, std::chrono::seconds::period>( seconds_to_record ));
			time_recordings.clear();
		}

		ImGui::SameLine();
#endif

		float settingsButtonWidth = ImGui::CalcTextSize( "Settings" ).x + ImGui::GetStyle().FramePadding.x * 2.f;
		ImGui::SetCursorPosX( ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - settingsButtonWidth );
		if ( ImGui::Button( "Settings" ) )
		{
			ImGui::OpenPopup( "Settings##Perf Settings Popup" );
		}

		if ( ImGui::BeginPopupModal( "Settings##Perf Settings Popup" ) )
		{
			float avg = 1.f - settings.running_avg_mult;
			ImGui::SliderFloat( "Running Average Multiplier", &avg, 0.0001f, 1.f, "%.5f", ImGuiSliderFlags_Logarithmic );
			settings.running_avg_mult = 1.f - avg;
			ImGui::SliderFloat( "White to Yellow Value (ms)", &settings.yellow_start, 0.01f, 33.3f, "%.2f" );
			ImGui::SliderFloat( "100% Yellow Value (ms)", &settings.yellow_end, 0.01f, 33.3f, "%.2f" );
			if ( settings.yellow_end <= settings.yellow_start + 1e-7f )
			{
				settings.yellow_end = settings.yellow_start + 0.1;
			}
			ImGui::SliderFloat( "Yellow to Red Value (ms)", &settings.orange_start, 0.01f, 33.3f, "%.2f" );
			if ( settings.orange_start <= settings.yellow_end + 1e-7f )
			{
				settings.orange_start = settings.yellow_end + 0.1;
			}
			ImGui::SliderFloat( "100% Red Value (ms)", &settings.orange_end, 0.01f, 60.f, "%.2f" );
			if ( settings.orange_end <= settings.orange_start + 1e-7f )
			{
				settings.orange_end = settings.orange_start + 0.1;
			}
			if ( ImGui::Button( "Ok" ) )
			{
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

#ifdef CHAG_USE_CUDA
		if ( ImGui::BeginTable( "performance", 4, ImGuiTableFlags_RowBg ) )
#else
		if ( ImGui::BeginTable( "performance", 3, ImGuiTableFlags_RowBg ) )
#endif
		{
			ImGuiTableColumnFlags flags =
				ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoSort;
			ImGui::TableSetupColumn( "Event", flags | ImGuiTableColumnFlags_WidthStretch );

			flags |= ImGuiTableColumnFlags_WidthFixed;
			ImGui::TableSetupColumn( "   CPU", flags, 100 );
			ImGui::TableSetupColumn( "   OpenGL", flags, 100 );
#ifdef CHAG_USE_CUDA
			ImGui::TableSetupColumn( "   CUDA", flags, 100 );
#endif

			ImGui::TableHeadersRow();

			float s = ImGui::GetStyle().IndentSpacing;
			ImGui::PushStyleVar( ImGuiStyleVar_IndentSpacing, s / 2.5 );

			for ( auto& e : events )
			{
				draw_events( e, e.name );
			}

			ImGui::PopStyleVar();

			ImGui::EndTable();
		}

#if USE_FMT
		if ( copy_text )
		{
			SDL_SetClipboardText( printf_events().c_str() );
		}
#endif

#if RECORD_TIMINGS
		timestamp_t current_time = getTimestamp();
		if ( remaining_recording_seconds.count() > 0 )
		{
			record_events();

			remaining_recording_seconds -= (current_time - last_frame_time);

			if ( remaining_recording_seconds.count() <= 0 )
			{
				// Save
				std::stringstream srec;
				for ( const auto& evt_type : time_recordings )
				{
					srec << '"' << evt_type.first << ":cpu" << '"';
					srec << "=[";
					for ( const auto& evt : evt_type.second )
					{
						float s = evt.cpu.count() / 1'000'000.f;
						srec << s << ',';
					}
					srec.seekp( -1, std::ios_base::end );
					srec << "]\n";

					srec << '"' << evt_type.first << ":gl" << '"';
					srec << "=[";
					for ( const auto& evt : evt_type.second )
					{
						float s = evt.gl.count() / 1'000'000.f;
						srec << s << ',';
					}
					srec.seekp( -1, std::ios_base::end );
					srec << "]\n";

					srec << '"' << evt_type.first << ":cuda" << '"';
					srec << "=[";
					for ( const auto& evt : evt_type.second )
					{
						float s = evt.cuda.count() / 1'000'000.f;
						srec << s << ',';
					}
					srec.seekp( -1, std::ios_base::end );
					srec << "]\n";

				}

				std::ofstream f( "perf.txt" );
				f.write( srec.str().c_str(), srec.str().length() );
			}
		}
		last_frame_time = current_time;
#endif
	}
	ImGui::End();

	std::swap( time_running_avg, time_running_avg_tmp );
	time_running_avg_tmp.clear();
	events.clear();
}

}
}   // namespace chag::perf


namespace labhelper
{
namespace perf
{
namespace cpu
{
void start_timer( time_event_t& e )
{
}

void stop_timer( time_event_t& e )
{
	timestamp_t t = getTimestamp();
	e.duration.cpu = t - e.start;
}

void sync()
{
}

}
}
}   // namespace chag::perf::cpu

namespace labhelper
{
namespace perf
{
namespace gl
{
namespace
{
struct event_t
{
	uint32_t start;
	uint32_t end;
};

std::vector<uint32_t> query_pool;
}   // namespace

uint32_t alloc_query()
{
	uint32_t e;
	if ( query_pool.empty() )
	{
		query_pool.resize( 256 );
		glGenQueries( query_pool.size(), query_pool.data() );
	}
	e = query_pool.back();
	query_pool.pop_back();
	return e;
}

void free_query( uint32_t q )
{
	query_pool.push_back( q );
}

void start_timer( time_event_t& e )
{
	event_t evt;
	evt.start = alloc_query();
	evt.end = alloc_query();

	glQueryCounter( evt.start, GL_TIMESTAMP );

	e.gl_data = std::move( evt );
}

void stop_timer( time_event_t& e )
{
	event_t& evt = std::any_cast<event_t>(e.gl_data);
	glQueryCounter( evt.end, GL_TIMESTAMP );
}

void sync()
{
	glFlush();

	std::vector<time_event_t*> rstack;

	for ( auto& e : events )
	{
		rstack.push_back( &e );
	}

	while ( !rstack.empty() )
	{
		time_event_t* e = rstack.back();
		rstack.pop_back();
		for ( auto& c : e->children )
		{
			rstack.push_back( &c );
		}

		event_t& ce = std::any_cast<event_t>(e->gl_data);

		uint64_t start;
		uint64_t end;

		glGetQueryObjectui64v( ce.start, GL_QUERY_RESULT, &start );
		glGetQueryObjectui64v( ce.end, GL_QUERY_RESULT, &end );

		e->duration.gl = std::chrono::nanoseconds( end - start );

		free_query( ce.start );
		free_query( ce.end );
	}
}

}
}
}   // namespace chag::perf::gl


#ifdef CHAG_USE_CUDA

#include <cuda_runtime.h>

#define checkCudaErr( expr )                                                                            \
	do                                                                                                  \
	{                                                                                                   \
		auto _status = expr;                                                                            \
		if ( _status != 0 )                                                                             \
		{                                                                                               \
			cudaDeviceReset();                                                                          \
			LOG_FATAL( "CUDA error in {}({}): {}", __FILE__, __LINE__, cudaGetErrorString( _status ) ); \
		}                                                                                               \
	} while ( 0 );


namespace chag::perf::cuda
{
namespace
{
struct event_t
{
	cudaEvent_t start;
	cudaEvent_t end;
};

std::vector<cudaEvent_t> event_pool;

cudaEvent_t last_recorded_event = nullptr;

void flushCUDA()
{
	if ( last_recorded_event != nullptr )
	{
		// This is most probably unnecessary since deviceSynch is supposed to synch *everything*
		checkCudaErr( cudaEventSynchronize( last_recorded_event ) );
		last_recorded_event = nullptr;
	}

	checkCudaErr( cudaDeviceSynchronize() );
}

}   // namespace

cudaEvent_t alloc_event()
{
	cudaEvent_t e;
	if ( event_pool.empty() )
	{
		checkCudaErr( cudaEventCreate( &e ) );
	}
	else
	{
		e = event_pool.back();
		event_pool.pop_back();
	}
	return e;
}

void free_event( cudaEvent_t evt )
{
	event_pool.push_back( evt );
}


void start_timer( time_event_t& e )
{
	event_t evt;
	evt.start = alloc_event();
	evt.end = alloc_event();

	cudaEventRecord( evt.start );
	last_recorded_event = evt.start;

	e.cuda_data = std::move( evt );
}

void stop_timer( time_event_t& e )
{
	event_t& evt = std::any_cast<event_t>(e.cuda_data);
	cudaEventRecord( evt.end );
	last_recorded_event = evt.end;
}

void sync()
{
	flushCUDA();

	std::vector<time_event_t*> rstack;
	for ( auto& e : events )
	{
		rstack.push_back( &e );
	}

	while ( !rstack.empty() )
	{
		time_event_t* e = rstack.back();
		rstack.pop_back();
		for ( auto& c : e->children )
		{
			rstack.push_back( &c );
		}

		float t_ms;
		event_t& ce = std::any_cast<event_t>(e->cuda_data);
		checkCudaErr( cudaEventElapsedTime( &t_ms, ce.start, ce.end ) );
		;

		e->duration.cuda = duration_t( uint64_t( double( t_ms ) * 1'000'000ui64 ) );

		free_event( ce.start );
		free_event( ce.end );
	}
}

}   // namespace chag::perf::cuda
#else

void labhelper::perf::cuda::start_timer( time_event_t& e )
{
}
void labhelper::perf::cuda::stop_timer( time_event_t& e )
{
}
void labhelper::perf::cuda::sync()
{
}
static void flushCUDA()
{
}

#endif   // CHAG_USE_CUDA
