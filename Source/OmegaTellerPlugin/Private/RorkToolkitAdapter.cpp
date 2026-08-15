// Copyright Epic Games, Inc. All Rights Reserved.

#include "RorkToolkitAdapter.h"
#include "OmegaTellerManager.h"
#include "OmegaCharacter.h"
#include "OmegaTree.h"
#include "OmegaNode.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformTime.h"

const FString URorkToolkitAdapter::DEFAULT_ENDPOINT = TEXT("https://api.rorktoolkit.com/v1/");
const FString URorkToolkitAdapter::DEFAULT_MODEL = TEXT("narrative-generator");
const float URorkToolkitAdapter::CACHE_DURATION_SECONDS = 300.0f; // 5 minutes

URorkToolkitAdapter::URorkToolkitAdapter()
	: APIKey(TEXT(""))
	, APIEndpoint(URorkToolkitAdapter::DEFAULT_ENDPOINT)
	, ModelName(URorkToolkitAdapter::DEFAULT_MODEL)
	, LastError(TEXT(""))
	, ConfidenceScore(0.0f)
	, bInitialized(false)
	, bAvailable(false)
{
}

void URorkToolkitAdapter::BeginDestroy()
{
	ResponseCache.Empty();
	CacheTimestamps.Empty();
	Super::BeginDestroy();
}

void URorkToolkitAdapter::Initialize_Implementation(const FString& ConfigPath)
{
	bAvailable = true;
	bInitialized = true;
	ConfidenceScore = 0.5f;
	UE_LOG(LogTemp, Log, TEXT("Rork Toolkit adapter initialized"));
}

bool URorkToolkitAdapter::IsAvailable_Implementation() const
{
	return bAvailable;
}

FString URorkToolkitAdapter::GetAIType_Implementation() const
{
	return TEXT("RorkToolkit");
}

// ============================================================================
// Helper: Normalize text - collapse whitespace, normalize line endings
// ============================================================================
static FString NormalizeText(const FString& InText)
{
	FString Result = InText;
	Result.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
	Result.ReplaceInline(TEXT("\r"), TEXT("\n"));

	// Collapse runs of spaces in a single pass (a repeated ReplaceInline loop is
	// quadratic and can hang the editor on large pasted documents)
	FString Collapsed;
	Collapsed.Reserve(Result.Len());
	bool bPrevWasSpace = false;
	for (int32 i = 0; i < Result.Len(); ++i)
	{
		const TCHAR Ch = Result[i];
		const bool bIsSpace = (Ch == TEXT(' '));
		if (!bIsSpace || !bPrevWasSpace)
		{
			Collapsed.AppendChar(Ch);
		}
		bPrevWasSpace = bIsSpace;
	}

	return Collapsed.TrimStartAndEnd();
}

// ============================================================================
// Helper: Check if a line is a markdown header (# ## ### etc.)
// ============================================================================
static bool IsMarkdownHeader(const FString& Line)
{
	FString Trimmed = Line.TrimStartAndEnd();
	return Trimmed.StartsWith(TEXT("#"));
}

// ============================================================================
// Helper: Extract the header content after # symbols
// ============================================================================
static FString GetHeaderContent(const FString& Line)
{
	FString Trimmed = Line.TrimStartAndEnd();
	while (Trimmed.StartsWith(TEXT("#")))
	{
		Trimmed.RemoveFromStart(TEXT("#"));
	}
	return Trimmed.TrimStartAndEnd();
}

// ============================================================================
// Helper: Get the header level (number of # symbols)
// ============================================================================
static int32 GetHeaderLevel(const FString& Line)
{
	FString Trimmed = Line.TrimStartAndEnd();
	int32 Level = 0;
	for (int32 i = 0; i < Trimmed.Len() && Trimmed[i] == TEXT('#'); ++i)
	{
		Level++;
	}
	return Level;
}

// ============================================================================
// Helper: Try to parse a numbered line "N. text" or "N) text"
// Returns true if it matches, and fills OutText with the content after the number
// ============================================================================
static bool TryParseNumberedLine(const FString& Line, FString& OutText)
{
	FString Trimmed = Line.TrimStartAndEnd();
	// Strip leading "- " for nested lists like "- 1. text"
	if (Trimmed.StartsWith(TEXT("- ")))
	{
		Trimmed = Trimmed.Mid(2).TrimStartAndEnd();
	}

	// Look for "N. " or "N) " where N is 1-3 digits
	for (int32 i = 0; i < Trimmed.Len() && i < 4; ++i)
	{
		TCHAR Ch = Trimmed[i];
		if (FChar::IsDigit(Ch))
		{
			continue;
		}
		if ((Ch == TEXT('.') || Ch == TEXT(')')) && i > 0 && i + 1 < Trimmed.Len())
		{
			FString NumPart = Trimmed.Left(i);
			if (NumPart.IsNumeric())
			{
				OutText = Trimmed.Mid(i + 1).TrimStartAndEnd();
				return !OutText.IsEmpty();
			}
		}
		break;
	}
	return false;
}

// ============================================================================
// Helper: Try to parse a bulleted line "- text"
// Returns true if it matches and the content isn't a "Key: Value" attribute
// ============================================================================
static bool TryParseBulletLine(const FString& Line, FString& OutText)
{
	FString Trimmed = Line.TrimStartAndEnd();
	if (!Trimmed.StartsWith(TEXT("- ")))
	{
		return false;
	}
	FString Content = Trimmed.Mid(2).TrimStartAndEnd();
	if (Content.IsEmpty())
	{
		return false;
	}

	// Check if this is a "Key: Value" attribute line (not a beat)
	int32 ColonPos = INDEX_NONE;
	Content.FindChar(TEXT(':'), ColonPos);
	if (ColonPos != INDEX_NONE && ColonPos > 0 && ColonPos < 25)
	{
		FString Key = Content.Left(ColonPos).TrimStartAndEnd();
		// Known attribute keys � these are NOT beats
		static const TSet<FString> AttributeKeys = {
			TEXT("Role"), TEXT("Personality"), TEXT("Goals"), TEXT("Goal"),
			TEXT("Motivation"), TEXT("Background"), TEXT("Description"),
			TEXT("Age"), TEXT("Class"), TEXT("Race"), TEXT("Trait"), TEXT("Traits"),
			TEXT("Occupation"), TEXT("Weakness"), TEXT("Strength"), TEXT("Backstory"),
			TEXT("Appearance"), TEXT("Alignment"), TEXT("Species"), TEXT("Gender"),
			TEXT("Name"), TEXT("Title"), TEXT("Weapon"), TEXT("Ability"),
		};
		for (const FString& AK : AttributeKeys)
		{
			if (Key.Equals(AK, ESearchCase::IgnoreCase))
			{
				return false;
			}
		}
	}

	OutText = Content;
	return true;
}

// ============================================================================
// Helper: Split text into sentences (handles . ! ? and newlines)
// ============================================================================
static TArray<FString> SplitIntoSentences(const FString& InText)
{
	TArray<FString> Sentences;
	FString Normalized = NormalizeText(InText);

	TArray<FString> Lines;
	Normalized.ParseIntoArray(Lines, TEXT("\n"), true);

	for (const FString& Line : Lines)
	{
		FString Trimmed = Line.TrimStartAndEnd();
		if (Trimmed.IsEmpty()) continue;

		// Skip pure header lines � they are structural, not sentences
		if (IsMarkdownHeader(Trimmed))
		{
			continue;
		}

		// Strip list markers for sentence extraction only
		FString ForSentences = Trimmed;
		if (ForSentences.StartsWith(TEXT("- ")))
		{
			ForSentences = ForSentences.Mid(2).TrimStartAndEnd();
		}
		else
		{
			FString NumText;
			if (TryParseNumberedLine(ForSentences, NumText))
			{
				ForSentences = NumText;
			}
		}

		if (ForSentences.IsEmpty()) continue;

		// Split by sentence-ending punctuation
		FString Current;
		for (int32 i = 0; i < ForSentences.Len(); ++i)
		{
			TCHAR Ch = ForSentences[i];
			Current.AppendChar(Ch);

			if ((Ch == TEXT('.') || Ch == TEXT('!') || Ch == TEXT('?'))
				&& (i + 1 >= ForSentences.Len() || ForSentences[i + 1] == TEXT(' ')))
			{
				FString Sentence = Current.TrimStartAndEnd();
				if (Sentence.Len() > 3)
				{
					Sentences.Add(Sentence);
				}
				Current.Empty();
			}
		}

		FString Remaining = Current.TrimStartAndEnd();
		if (Remaining.Len() > 3)
		{
			Sentences.Add(Remaining);
		}
	}

	return Sentences;
}

// ============================================================================
// Helper: Check if a word looks like a proper noun
// ============================================================================
static bool IsLikelyProperNoun(const FString& Word)
{
	if (Word.Len() < 2) return false;
	if (!FChar::IsUpper(Word[0])) return false;

	static const TSet<FString> CommonWords = {
		TEXT("The"), TEXT("This"), TEXT("That"), TEXT("They"), TEXT("There"),
		TEXT("Then"), TEXT("These"), TEXT("Those"), TEXT("Their"), TEXT("Them"),
		TEXT("When"), TEXT("Where"), TEXT("What"), TEXT("Which"), TEXT("While"),
		TEXT("With"), TEXT("Will"), TEXT("Would"), TEXT("Could"), TEXT("Should"),
		TEXT("Have"), TEXT("Has"), TEXT("Had"), TEXT("Been"), TEXT("Being"),
		TEXT("Into"), TEXT("From"), TEXT("Each"), TEXT("Every"), TEXT("Some"),
		TEXT("Many"), TEXT("Much"), TEXT("More"), TEXT("Most"), TEXT("Must"),
		TEXT("Also"), TEXT("Both"), TEXT("Such"), TEXT("Very"), TEXT("Just"),
		TEXT("Game"), TEXT("Design"), TEXT("Document"), TEXT("Story"), TEXT("Plot"),
		TEXT("Chapter"), TEXT("Section"), TEXT("Part"), TEXT("Act"), TEXT("Scene"),
		TEXT("Characters"), TEXT("Character"), TEXT("Role"), TEXT("Roles"),
		TEXT("Personality"), TEXT("Goals"), TEXT("Goal"), TEXT("Objective"),
		TEXT("Beginning"), TEXT("Middle"), TEXT("End"), TEXT("Final"),
		TEXT("Start"), TEXT("Ending"), TEXT("Starts"), TEXT("Ends"),
		TEXT("But"), TEXT("And"), TEXT("For"), TEXT("Not"), TEXT("You"),
		TEXT("All"), TEXT("Can"), TEXT("Her"), TEXT("His"), TEXT("How"),
		TEXT("Its"), TEXT("May"), TEXT("New"), TEXT("Now"), TEXT("Old"),
		TEXT("See"), TEXT("Way"), TEXT("Who"), TEXT("Did"), TEXT("Get"),
		TEXT("Let"), TEXT("Say"), TEXT("She"), TEXT("Too"), TEXT("Use"),
		TEXT("After"), TEXT("Before"), TEXT("During"), TEXT("About"),
		TEXT("Between"), TEXT("Through"), TEXT("Against"), TEXT("Along"),
		TEXT("Among"), TEXT("Around"), TEXT("Because"), TEXT("Beyond"),
		TEXT("However"), TEXT("Therefore"), TEXT("Although"), TEXT("Meanwhile"),
		TEXT("Eventually"), TEXT("Finally"), TEXT("Instead"), TEXT("Overall"),
		TEXT("Perhaps"), TEXT("Suddenly"), TEXT("Throughout"), TEXT("Together"),
		TEXT("Example"), TEXT("Note"), TEXT("Description"), TEXT("Overview"),
		TEXT("Summary"), TEXT("Introduction"), TEXT("Conclusion"),
		TEXT("Player"), TEXT("Players"), TEXT("World"), TEXT("Kingdom"),
		TEXT("Quest"), TEXT("Quests"), TEXT("Level"), TEXT("Levels"),
		TEXT("Battle"), TEXT("Combat"), TEXT("Fight"), TEXT("Weapon"),
		TEXT("Magic"), TEXT("Power"), TEXT("Skill"), TEXT("Skills"),
		TEXT("Ancient"), TEXT("Dark"), TEXT("Light"), TEXT("Evil"),
		TEXT("Good"), TEXT("Great"), TEXT("True"), TEXT("False"),
		TEXT("Beats"), TEXT("Setting"), TEXT("Mechanics"), TEXT("Gameplay"),
		TEXT("Only"), TEXT("Free"), TEXT("Seat"), TEXT("Next"),
	};

	return !CommonWords.Contains(Word);
}

// ============================================================================
// Helper: Known section header names that are NOT character names
// ============================================================================
static bool IsSectionHeaderName(const FString& Name)
{
	static const TSet<FString> SectionNames = {
		TEXT("Characters"), TEXT("Story"), TEXT("Story Beats"),
		TEXT("Gameplay"), TEXT("Mechanics"), TEXT("World"), TEXT("Setting"),
		TEXT("Beats"), TEXT("Overview"), TEXT("Introduction"), TEXT("Conclusion"),
		TEXT("Prologue"), TEXT("Epilogue"), TEXT("Synopsis"), TEXT("Summary"),
		TEXT("Background"), TEXT("Lore"), TEXT("Objectives"), TEXT("Quests"),
	};
	return SectionNames.Contains(Name);
}

// ============================================================================
// Struct: Parsed GDD structure
// ============================================================================
struct FParsedGDDSection
{
	FString SectionName;
	int32 HeaderLevel;
	TArray<FString> NumberedBeats;   // "1. text" items
	TArray<FString> BulletItems;     // "- text" items (non-attribute)
	TArray<TPair<FString, FString>> Attributes; // "- Key: Value" items
	TArray<FString> FreeText;        // other lines
};

// ============================================================================
// Helper: Parse the GDD into structured sections
// ============================================================================
static TArray<FParsedGDDSection> ParseGDDSections(const FString& GDDText)
{
	TArray<FParsedGDDSection> Sections;
	FString Normalized = NormalizeText(GDDText);
	TArray<FString> Lines;
	Normalized.ParseIntoArray(Lines, TEXT("\n"), true);

	// Always start with a root section for content before any header
	FParsedGDDSection* CurrentSection = nullptr;
	{
		FParsedGDDSection Root;
		Root.SectionName = TEXT("_root");
		Root.HeaderLevel = 0;
		Sections.Add(Root);
		CurrentSection = &Sections.Last();
	}

	for (const FString& Line : Lines)
	{
		FString Trimmed = Line.TrimStartAndEnd();
		if (Trimmed.IsEmpty()) continue;

		if (IsMarkdownHeader(Trimmed))
		{
			FParsedGDDSection NewSection;
			NewSection.SectionName = GetHeaderContent(Trimmed);
			NewSection.HeaderLevel = GetHeaderLevel(Trimmed);
			Sections.Add(NewSection);
			CurrentSection = &Sections.Last();
			continue;
		}

		// Try numbered line
		FString NumText;
		if (TryParseNumberedLine(Trimmed, NumText))
		{
			CurrentSection->NumberedBeats.Add(NumText);
			continue;
		}

		// Try bullet line
		FString BulletText;
		if (TryParseBulletLine(Trimmed, BulletText))
		{
			CurrentSection->BulletItems.Add(BulletText);
			continue;
		}

		// Check for attribute "- Key: Value"
		if (Trimmed.StartsWith(TEXT("- ")))
		{
			FString Content = Trimmed.Mid(2).TrimStartAndEnd();
			int32 ColonPos = INDEX_NONE;
			Content.FindChar(TEXT(':'), ColonPos);
			if (ColonPos != INDEX_NONE && ColonPos > 0)
			{
				FString Key = Content.Left(ColonPos).TrimStartAndEnd();
				FString Value = Content.Mid(ColonPos + 1).TrimStartAndEnd();
				if (!Key.IsEmpty() && !Value.IsEmpty() && Key.Len() < 30)
				{
					CurrentSection->Attributes.Add(TPair<FString, FString>(Key, Value));
					continue;
				}
			}
		}

		// Free text
		if (Trimmed.Len() > 3)
		{
			CurrentSection->FreeText.Add(Trimmed);
		}
	}

	return Sections;
}

// ============================================================================
// Helper: Extract candidate character names from text
// ============================================================================
struct FCharacterCandidate
{
	FString Name;
	int32 MentionCount;
	TArray<FString> ContextSentences;
	TMap<FString, FString> ExplicitAttributes; // from "- Key: Value" lines
	TArray<FString> DirectBeats; // beats from the character's own section

	FCharacterCandidate() : MentionCount(0) {}
};

static TMap<FString, FCharacterCandidate> ExtractCharacterCandidates(const FString& InText, const TArray<FString>& Sentences)
{
	TMap<FString, FCharacterCandidate> Candidates;

	// Parse structured sections
	TArray<FParsedGDDSection> Sections = ParseGDDSections(InText);

	// ?? Strategy 1: Find characters from ### headers under a "Characters" section ??
	// Also collect their attributes from the lines below
	bool bInCharactersSection = false;
	for (int32 si = 0; si < Sections.Num(); ++si)
	{
		const FParsedGDDSection& Sec = Sections[si];

		if (IsSectionHeaderName(Sec.SectionName))
		{
			bInCharactersSection = Sec.SectionName.Equals(TEXT("Characters"), ESearchCase::IgnoreCase);
			continue;
		}

		// ### headers (level 3) are character definitions under "## Characters"
		// ## headers (level 2) that are NOT known sections could be character names
		//   OR character-specific beat sections
		bool bIsCharDef = false;
		if (Sec.HeaderLevel >= 3 && bInCharactersSection)
		{
			bIsCharDef = true;
		}
		else if (Sec.HeaderLevel >= 2 && !IsSectionHeaderName(Sec.SectionName))
		{
			// Could be a character name section � check if it has attributes or beats
			if (Sec.Attributes.Num() > 0 || Sec.NumberedBeats.Num() > 0)
			{
				bIsCharDef = true;
			}
		}

		if (bIsCharDef && !Sec.SectionName.IsEmpty())
		{
			FCharacterCandidate& Candidate = Candidates.FindOrAdd(Sec.SectionName);
			Candidate.Name = Sec.SectionName;
			Candidate.MentionCount += 10; // high confidence from header definition

			// Collect explicit attributes
			for (const auto& Attr : Sec.Attributes)
			{
				Candidate.ExplicitAttributes.Add(Attr.Key, Attr.Value);
			}

			// Collect numbered beats from this character's section
			for (const FString& Beat : Sec.NumberedBeats)
			{
				Candidate.DirectBeats.Add(Beat);
			}

			// Collect bullet items as beats too (if no numbered beats)
			if (Sec.NumberedBeats.Num() == 0)
			{
				for (const FString& Bullet : Sec.BulletItems)
				{
					Candidate.DirectBeats.Add(Bullet);
				}
			}
		}
	}

	// ?? Strategy 2: Find ## CharacterName sections that have numbered beats ??
	// These are beat-sections, not character-definition sections
	// (e.g., "## John" followed by "1. John wakes up...")
	for (const FParsedGDDSection& Sec : Sections)
	{
		if (Sec.HeaderLevel >= 2 && !IsSectionHeaderName(Sec.SectionName)
			&& !Sec.SectionName.IsEmpty() && Sec.NumberedBeats.Num() > 0)
		{
			FCharacterCandidate& Candidate = Candidates.FindOrAdd(Sec.SectionName);
			Candidate.Name = Sec.SectionName;
			if (Candidate.MentionCount == 0)
			{
				Candidate.MentionCount += 8;
			}

			// Add beats (avoid duplicates)
			for (const FString& Beat : Sec.NumberedBeats)
			{
				if (!Candidate.DirectBeats.Contains(Beat))
				{
					Candidate.DirectBeats.Add(Beat);
				}
			}
		}
	}

	// ?? Strategy 3: NLP extraction from free-form text ??
	TMap<FString, int32> ProperNounSequences;

	for (const FString& Sentence : Sentences)
	{
		TArray<FString> Words;
		Sentence.ParseIntoArrayWS(Words);

		for (int32 i = 0; i < Words.Num(); ++i)
		{
			FString CleanWord = Words[i];
			CleanWord.ReplaceInline(TEXT(","), TEXT(""));
			CleanWord.ReplaceInline(TEXT("."), TEXT(""));
			CleanWord.ReplaceInline(TEXT("!"), TEXT(""));
			CleanWord.ReplaceInline(TEXT("?"), TEXT(""));
			CleanWord.ReplaceInline(TEXT(":"), TEXT(""));
			CleanWord.ReplaceInline(TEXT(";"), TEXT(""));
			CleanWord.ReplaceInline(TEXT("\""), TEXT(""));
			CleanWord.ReplaceInline(TEXT("'"), TEXT(""));

			if (CleanWord.IsEmpty()) continue;

			bool bIsFirstWord = (i == 0);

			if (IsLikelyProperNoun(CleanWord))
			{
				FString FullName = CleanWord;
				int32 j = i + 1;
				while (j < Words.Num())
				{
					FString NextWord = Words[j];
					NextWord.ReplaceInline(TEXT(","), TEXT(""));
					NextWord.ReplaceInline(TEXT("."), TEXT(""));
					NextWord.ReplaceInline(TEXT("!"), TEXT(""));
					NextWord.ReplaceInline(TEXT("?"), TEXT(""));

					if (NextWord.Len() > 1 && FChar::IsUpper(NextWord[0]) && IsLikelyProperNoun(NextWord))
					{
						FullName += TEXT(" ") + NextWord;
						++j;
					}
					else
					{
						break;
					}
				}

				if (FullName.Contains(TEXT(" ")) || !bIsFirstWord)
				{
					ProperNounSequences.FindOrAdd(FullName)++;
				}
			}
		}
	}

	// Add proper noun sequences with enough mentions as candidates
	for (const auto& Pair : ProperNounSequences)
	{
		bool bIsMultiWord = Pair.Key.Contains(TEXT(" "));
		int32 MinMentions = bIsMultiWord ? 1 : 2;

		if (Pair.Value >= MinMentions && !Candidates.Contains(Pair.Key))
		{
			FCharacterCandidate& Candidate = Candidates.FindOrAdd(Pair.Key);
			Candidate.Name = Pair.Key;
			Candidate.MentionCount += Pair.Value;
		}
	}

	// ?? Strategy 4: Character-introduction patterns ??
	for (const FString& Sentence : Sentences)
	{
		// "X is a/an [role]" pattern
		int32 IsAPos = Sentence.Find(TEXT(" is a "), ESearchCase::IgnoreCase);
		if (IsAPos == INDEX_NONE)
			IsAPos = Sentence.Find(TEXT(" is an "), ESearchCase::IgnoreCase);
		if (IsAPos == INDEX_NONE)
			IsAPos = Sentence.Find(TEXT(" is the "), ESearchCase::IgnoreCase);

		if (IsAPos != INDEX_NONE && IsAPos > 0)
		{
			FString BeforeIs = Sentence.Left(IsAPos).TrimStartAndEnd();
			TArray<FString> BeforeWords;
			BeforeIs.ParseIntoArrayWS(BeforeWords);

			FString CandidateName;
			for (int32 k = BeforeWords.Num() - 1; k >= 0; --k)
			{
				FString W = BeforeWords[k];
				W.ReplaceInline(TEXT(","), TEXT(""));
				if (W.Len() > 1 && FChar::IsUpper(W[0]))
				{
					CandidateName = CandidateName.IsEmpty() ? W : (W + TEXT(" ") + CandidateName);
				}
				else
				{
					break;
				}
			}

			if (!CandidateName.IsEmpty())
			{
				FString FirstWord = CandidateName;
				int32 SpaceIdx = INDEX_NONE;
				CandidateName.FindChar(TEXT(' '), SpaceIdx);
				if (SpaceIdx != INDEX_NONE)
				{
					FirstWord = CandidateName.Left(SpaceIdx);
				}
				if (IsLikelyProperNoun(FirstWord))
				{
					FCharacterCandidate& Candidate = Candidates.FindOrAdd(CandidateName);
					Candidate.Name = CandidateName;
					Candidate.MentionCount += 3;
				}
			}
		}

		// "named X" or "called X" pattern
		for (const FString& Keyword : {FString(TEXT("named ")), FString(TEXT("called "))})
		{
			int32 KwPos = Sentence.Find(Keyword, ESearchCase::IgnoreCase);
			if (KwPos != INDEX_NONE)
			{
				FString After = Sentence.Mid(KwPos + Keyword.Len()).TrimStartAndEnd();
				TArray<FString> AfterWords;
				After.ParseIntoArrayWS(AfterWords);

				FString CandidateName;
				for (const FString& W : AfterWords)
				{
					FString Clean = W;
					Clean.ReplaceInline(TEXT(","), TEXT(""));
					Clean.ReplaceInline(TEXT("."), TEXT(""));
					if (Clean.Len() > 1 && FChar::IsUpper(Clean[0]))
					{
						if (!CandidateName.IsEmpty()) CandidateName += TEXT(" ");
						CandidateName += Clean;
					}
					else
					{
						break;
					}
				}

				if (!CandidateName.IsEmpty())
				{
					FCharacterCandidate& Candidate = Candidates.FindOrAdd(CandidateName);
					Candidate.Name = CandidateName;
					Candidate.MentionCount += 3;
				}
			}
		}
	}

	// ?? Collect context sentences for each candidate ??
	for (auto& CandidatePair : Candidates)
	{
		for (const FString& Sentence : Sentences)
		{
			if (Sentence.Contains(CandidatePair.Key))
			{
				CandidatePair.Value.ContextSentences.Add(Sentence);
			}
		}
	}

	// ?? Deduplicate: merge short names into longer ones ??
	TArray<FString> ToRemove;
	for (const auto& ShortPair : Candidates)
	{
		for (const auto& LongPair : Candidates)
		{
			if (ShortPair.Key != LongPair.Key
				&& LongPair.Key.Contains(ShortPair.Key)
				&& LongPair.Key.Len() > ShortPair.Key.Len())
			{
				Candidates[LongPair.Key].MentionCount += ShortPair.Value.MentionCount;
				// Merge direct beats
				for (const FString& Beat : ShortPair.Value.DirectBeats)
				{
					if (!Candidates[LongPair.Key].DirectBeats.Contains(Beat))
					{
						Candidates[LongPair.Key].DirectBeats.Add(Beat);
					}
				}
				ToRemove.AddUnique(ShortPair.Key);
			}
		}
	}
	for (const FString& Key : ToRemove)
	{
		Candidates.Remove(Key);
	}

	return Candidates;
}

// ============================================================================
// Helper: Extract attributes for a character from context sentences
// ============================================================================
static TMap<FString, FString> InferCharacterAttributes(const FString& CharacterName, const TArray<FString>& ContextSentences, const FString& FullText)
{
	TMap<FString, FString> Attributes;

	static const TMap<FString, FString> RoleKeywords = {
		{TEXT("protagonist"), TEXT("Protagonist")},
		{TEXT("hero"), TEXT("Protagonist")},
		{TEXT("heroine"), TEXT("Protagonist")},
		{TEXT("main character"), TEXT("Protagonist")},
		{TEXT("antagonist"), TEXT("Antagonist")},
		{TEXT("villain"), TEXT("Antagonist")},
		{TEXT("enemy"), TEXT("Antagonist")},
		{TEXT("rival"), TEXT("Antagonist")},
		{TEXT("ally"), TEXT("Ally")},
		{TEXT("companion"), TEXT("Companion")},
		{TEXT("mentor"), TEXT("Mentor")},
		{TEXT("guide"), TEXT("Mentor")},
		{TEXT("teacher"), TEXT("Mentor")},
		{TEXT("friend"), TEXT("Ally")},
		{TEXT("partner"), TEXT("Ally")},
		{TEXT("king"), TEXT("Ruler")},
		{TEXT("queen"), TEXT("Ruler")},
		{TEXT("prince"), TEXT("Royalty")},
		{TEXT("princess"), TEXT("Royalty")},
		{TEXT("lord"), TEXT("Nobility")},
		{TEXT("lady"), TEXT("Nobility")},
		{TEXT("knight"), TEXT("Warrior")},
		{TEXT("warrior"), TEXT("Warrior")},
		{TEXT("wizard"), TEXT("Mage")},
		{TEXT("mage"), TEXT("Mage")},
		{TEXT("sorcerer"), TEXT("Mage")},
		{TEXT("sorceress"), TEXT("Mage")},
		{TEXT("thief"), TEXT("Rogue")},
		{TEXT("rogue"), TEXT("Rogue")},
		{TEXT("assassin"), TEXT("Rogue")},
		{TEXT("healer"), TEXT("Healer")},
		{TEXT("priest"), TEXT("Healer")},
		{TEXT("merchant"), TEXT("Merchant")},
		{TEXT("trader"), TEXT("Merchant")},
		{TEXT("student"), TEXT("Student")},
		{TEXT("barista"), TEXT("Barista")},
		{TEXT("npc"), TEXT("NPC")},
		{TEXT("boss"), TEXT("Boss")},
	};

	static const TMap<FString, FString> PersonalityKeywords = {
		{TEXT("brave"), TEXT("Brave")},
		{TEXT("courage"), TEXT("Courageous")},
		{TEXT("curious"), TEXT("Curious")},
		{TEXT("cunning"), TEXT("Cunning")},
		{TEXT("ambitious"), TEXT("Ambitious")},
		{TEXT("kind"), TEXT("Kind")},
		{TEXT("cruel"), TEXT("Cruel")},
		{TEXT("wise"), TEXT("Wise")},
		{TEXT("clever"), TEXT("Clever")},
		{TEXT("loyal"), TEXT("Loyal")},
		{TEXT("treacherous"), TEXT("Treacherous")},
		{TEXT("mysterious"), TEXT("Mysterious")},
		{TEXT("determined"), TEXT("Determined")},
		{TEXT("stubborn"), TEXT("Stubborn")},
		{TEXT("gentle"), TEXT("Gentle")},
		{TEXT("fierce"), TEXT("Fierce")},
		{TEXT("shy"), TEXT("Shy")},
		{TEXT("charismatic"), TEXT("Charismatic")},
		{TEXT("cold"), TEXT("Cold")},
		{TEXT("warm"), TEXT("Warm")},
		{TEXT("strategic"), TEXT("Strategic")},
		{TEXT("impulsive"), TEXT("Impulsive")},
		{TEXT("cautious"), TEXT("Cautious")},
		{TEXT("noble"), TEXT("Noble")},
		{TEXT("honorable"), TEXT("Honorable")},
		{TEXT("ruthless"), TEXT("Ruthless")},
		{TEXT("compassionate"), TEXT("Compassionate")},
		{TEXT("intelligent"), TEXT("Intelligent")},
		{TEXT("strong"), TEXT("Strong")},
		{TEXT("resourceful"), TEXT("Resourceful")},
	};

	// Look for explicit "Key: Value" lines near the character name
	TArray<FString> Lines;
	FullText.ParseIntoArray(Lines, TEXT("\n"), true);

	bool bInCharacterSection = false;
	for (const FString& Line : Lines)
	{
		FString Trimmed = Line.TrimStartAndEnd();

		if (Trimmed.Contains(CharacterName))
		{
			bInCharacterSection = true;
		}
		else if (IsMarkdownHeader(Trimmed))
		{
			if (bInCharacterSection)
			{
				bInCharacterSection = false;
			}
		}

		if (bInCharacterSection)
		{
			FString CleanLine = Trimmed;
			if (CleanLine.StartsWith(TEXT("-")))
			{
				CleanLine.RemoveFromStart(TEXT("-"));
				CleanLine = CleanLine.TrimStartAndEnd();
			}

			int32 ColonPos = INDEX_NONE;
			CleanLine.FindChar(TEXT(':'), ColonPos);
			if (ColonPos != INDEX_NONE && ColonPos > 0 && ColonPos < CleanLine.Len() - 1)
			{
				FString Key = CleanLine.Left(ColonPos).TrimStartAndEnd();
				FString Value = CleanLine.Mid(ColonPos + 1).TrimStartAndEnd();

				if (!Key.IsEmpty() && !Value.IsEmpty() && Key.Len() < 30)
				{
					Attributes.Add(Key, Value);
				}
			}
		}
	}

	if (!Attributes.Contains(TEXT("Role")))
	{
		FString InferredRole;
		for (const FString& Sentence : ContextSentences)
		{
			FString LowerSentence = Sentence.ToLower();
			for (const auto& RolePair : RoleKeywords)
			{
				if (LowerSentence.Contains(RolePair.Key))
				{
					InferredRole = RolePair.Value;
					break;
				}
			}
			if (!InferredRole.IsEmpty()) break;
		}

		if (InferredRole.IsEmpty())
		{
			FString LowerName = CharacterName.ToLower();
			for (const auto& RolePair : RoleKeywords)
			{
				if (LowerName.Contains(RolePair.Key))
				{
					InferredRole = RolePair.Value;
					break;
				}
			}
		}

		Attributes.Add(TEXT("Role"), InferredRole.IsEmpty() ? TEXT("Character") : InferredRole);
	}

	if (!Attributes.Contains(TEXT("Personality")))
	{
		TArray<FString> Traits;
		for (const FString& Sentence : ContextSentences)
		{
			FString LowerSentence = Sentence.ToLower();
			for (const auto& PersonPair : PersonalityKeywords)
			{
				if (LowerSentence.Contains(PersonPair.Key) && !Traits.Contains(PersonPair.Value))
				{
					Traits.Add(PersonPair.Value);
				}
			}
		}

		if (Traits.Num() > 0)
		{
			Attributes.Add(TEXT("Personality"), FString::Join(Traits, TEXT(", ")));
		}
	}

	if (!Attributes.Contains(TEXT("Goals")))
	{
		for (const FString& Sentence : ContextSentences)
		{
			FString LowerSentence = Sentence.ToLower();
			if (LowerSentence.Contains(TEXT("wants to"))
				|| LowerSentence.Contains(TEXT("seeks to"))
				|| LowerSentence.Contains(TEXT("aims to"))
				|| LowerSentence.Contains(TEXT("must"))
				|| LowerSentence.Contains(TEXT("goal"))
				|| LowerSentence.Contains(TEXT("objective"))
				|| LowerSentence.Contains(TEXT("mission"))
				|| LowerSentence.Contains(TEXT("quest"))
				|| LowerSentence.Contains(TEXT("try to"))
				|| LowerSentence.Contains(TEXT("tries to"))
				|| LowerSentence.Contains(TEXT("needs to"))
				|| LowerSentence.Contains(TEXT("desires"))
				|| LowerSentence.Contains(TEXT("driven by")))
			{
				Attributes.Add(TEXT("Goals"), Sentence);
				break;
			}
		}
	}

	return Attributes;
}

// ============================================================================
// ExtractCharactersFromGDD - Intelligent text analysis
// ============================================================================
TArray<UOmegaCharacter*> URorkToolkitAdapter::ExtractCharactersFromGDD_Implementation(const FString& GDDText)
{
	TArray<UOmegaCharacter*> Characters;

	UOmegaTellerManager* Manager = UOmegaTellerManager::Get();
	if (!Manager || GDDText.IsEmpty())
	{
		return Characters;
	}

	UE_LOG(LogTemp, Log, TEXT("ExtractCharactersFromGDD: Analyzing text of length %d"), GDDText.Len());

	TArray<FString> Sentences = SplitIntoSentences(GDDText);
	UE_LOG(LogTemp, Log, TEXT("ExtractCharactersFromGDD: Found %d sentences"), Sentences.Num());

	TMap<FString, FCharacterCandidate> Candidates = ExtractCharacterCandidates(GDDText, Sentences);
	UE_LOG(LogTemp, Log, TEXT("ExtractCharactersFromGDD: Found %d character candidates"), Candidates.Num());

	// Sort candidates by mention count (highest first)
	TArray<TPair<FString, FCharacterCandidate>> SortedCandidates;
	for (const auto& Pair : Candidates)
	{
		SortedCandidates.Add(TPair<FString, FCharacterCandidate>(Pair.Key, Pair.Value));
	}
	SortedCandidates.Sort([](const auto& A, const auto& B)
	{
		return A.Value.MentionCount > B.Value.MentionCount;
	});

	int32 MaxCharacters = FMath::Min(SortedCandidates.Num(), 20);
	for (int32 i = 0; i < MaxCharacters; ++i)
	{
		const FCharacterCandidate& Candidate = SortedCandidates[i].Value;

		UE_LOG(LogTemp, Log, TEXT("ExtractCharactersFromGDD: Creating character '%s' (mentions: %d, directBeats: %d)"),
			*Candidate.Name, Candidate.MentionCount, Candidate.DirectBeats.Num());

		UOmegaCharacter* NewCharacter = Manager->CreateCharacter(Candidate.Name);
		if (NewCharacter)
		{
			// First apply explicit attributes from structured parsing
			for (const auto& AttrPair : Candidate.ExplicitAttributes)
			{
				NewCharacter->UpdateAttribute(AttrPair.Key, AttrPair.Value);
			}

			// Then infer missing attributes from context
			TMap<FString, FString> InferredAttributes = InferCharacterAttributes(
				Candidate.Name, Candidate.ContextSentences, GDDText);

			for (const auto& AttrPair : InferredAttributes)
			{
				// Don't overwrite explicit attributes
				FString Existing = NewCharacter->GetAttribute(AttrPair.Key);
				if (Existing.IsEmpty())
				{
					NewCharacter->UpdateAttribute(AttrPair.Key, AttrPair.Value);
				}
			}

			Characters.Add(NewCharacter);
		}
	}

	// If no characters found, try aggressive extraction
	if (Characters.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ExtractCharactersFromGDD: No characters found, trying aggressive extraction"));

		TMap<FString, int32> AllProperNouns;
		for (const FString& Sentence : Sentences)
		{
			TArray<FString> Words;
			Sentence.ParseIntoArrayWS(Words);

			for (int32 wi = 0; wi < Words.Num(); ++wi)
			{
				FString Clean = Words[wi];
				Clean.ReplaceInline(TEXT(","), TEXT(""));
				Clean.ReplaceInline(TEXT("."), TEXT(""));
				Clean.ReplaceInline(TEXT("!"), TEXT(""));
				Clean.ReplaceInline(TEXT("?"), TEXT(""));

				if (Clean.Len() > 2 && FChar::IsUpper(Clean[0]) && IsLikelyProperNoun(Clean))
				{
					FString FullName = Clean;
					for (int32 j = wi + 1; j < Words.Num() && j <= wi + 3; ++j)
					{
						FString Next = Words[j];
						Next.ReplaceInline(TEXT(","), TEXT(""));
						Next.ReplaceInline(TEXT("."), TEXT(""));
						if (Next.Len() > 1 && FChar::IsUpper(Next[0]) && IsLikelyProperNoun(Next))
						{
							FullName += TEXT(" ") + Next;
						}
						else
						{
							break;
						}
					}
					AllProperNouns.FindOrAdd(FullName)++;
				}
			}
		}

		TArray<TPair<FString, int32>> SortedNouns;
		for (const auto& Pair : AllProperNouns)
		{
			SortedNouns.Add(TPair<FString, int32>(Pair.Key, Pair.Value));
		}
		SortedNouns.Sort([](const auto& A, const auto& B) { return A.Value > B.Value; });

		int32 AggressiveMax = FMath::Min(SortedNouns.Num(), 5);
		for (int32 ai = 0; ai < AggressiveMax; ++ai)
		{
			UOmegaCharacter* NewCharacter = Manager->CreateCharacter(SortedNouns[ai].Key);
			if (NewCharacter)
			{
				NewCharacter->UpdateAttribute(TEXT("Role"), TEXT("Character"));
				Characters.Add(NewCharacter);
			}
		}
	}

	ConfidenceScore = Characters.Num() > 0 ? FMath::Clamp(0.3f + (0.1f * Characters.Num()), 0.3f, 0.95f) : 0.1f;
	UE_LOG(LogTemp, Log, TEXT("ExtractCharactersFromGDD: Extracted %d characters (confidence: %.2f)"),
		Characters.Num(), ConfidenceScore);

	return Characters;
}

// ============================================================================
// ExtractStoryBeatsFromGDD - Section-aware beat extraction
// ============================================================================
TArray<FString> URorkToolkitAdapter::ExtractStoryBeatsFromGDD_Implementation(const FString& GDDText)
{
	TArray<FString> StoryBeats;

	if (GDDText.IsEmpty())
	{
		StoryBeats.Add(TEXT("Beginning"));
		StoryBeats.Add(TEXT("Middle"));
		StoryBeats.Add(TEXT("End"));
		return StoryBeats;
	}

	// Parse into structured sections
	TArray<FParsedGDDSection> Sections = ParseGDDSections(GDDText);

	// Collect all numbered beats from all sections (preserving order)
	for (const FParsedGDDSection& Sec : Sections)
	{
		for (const FString& Beat : Sec.NumberedBeats)
		{
			if (!StoryBeats.Contains(Beat))
			{
				StoryBeats.Add(Beat);
			}
		}
	}

	if (StoryBeats.Num() >= 2)
	{
		return StoryBeats;
	}

	// If no numbered beats, collect bullet items from beat-related sections
	StoryBeats.Empty();
	for (const FParsedGDDSection& Sec : Sections)
	{
		bool bIsBeatSection = Sec.SectionName.Contains(TEXT("Beat"), ESearchCase::IgnoreCase)
			|| Sec.SectionName.Contains(TEXT("Story"), ESearchCase::IgnoreCase)
			|| Sec.SectionName.Contains(TEXT("Plot"), ESearchCase::IgnoreCase)
			|| Sec.SectionName.Contains(TEXT("Timeline"), ESearchCase::IgnoreCase);

		if (bIsBeatSection)
		{
			for (const FString& Bullet : Sec.BulletItems)
			{
				if (!StoryBeats.Contains(Bullet))
				{
					StoryBeats.Add(Bullet);
				}
			}
		}
	}

	if (StoryBeats.Num() >= 2)
	{
		return StoryBeats;
	}

	// Fall back to keyword-based extraction from sentences
	StoryBeats.Empty();
	TArray<FString> Sentences = SplitIntoSentences(GDDText);

	static const TArray<FString> BeatKeywords = {
		TEXT("discover"), TEXT("finds"), TEXT("found"), TEXT("learn"),
		TEXT("confront"), TEXT("battle"), TEXT("fight"), TEXT("defeat"),
		TEXT("choose"), TEXT("decision"), TEXT("decide"),
		TEXT("betray"), TEXT("reveal"), TEXT("secret"),
		TEXT("journey"), TEXT("travel"), TEXT("arrive"),
		TEXT("meet"), TEXT("encounter"), TEXT("face"),
		TEXT("escape"), TEXT("capture"), TEXT("rescue"),
		TEXT("transform"), TEXT("change"), TEXT("evolve"),
		TEXT("sacrifice"), TEXT("die"), TEXT("death"),
		TEXT("win"), TEXT("lose"), TEXT("victory"),
		TEXT("alliance"), TEXT("join"), TEXT("unite"),
		TEXT("conflict"), TEXT("war"), TEXT("peace"),
		TEXT("quest"), TEXT("mission"), TEXT("task"),
		TEXT("begin"), TEXT("start"), TEXT("end"),
		TEXT("climax"), TEXT("resolution"), TEXT("twist"),
		TEXT("tension"), TEXT("rise"), TEXT("fall"),
		TEXT("must"), TEXT("needs to"), TEXT("has to"),
		TEXT("wakes"), TEXT("walks"), TEXT("gets"), TEXT("leaves"),
		TEXT("finishes"), TEXT("sits"), TEXT("meets"),
	};

	for (const FString& Sentence : Sentences)
	{
		FString LowerSentence = Sentence.ToLower();
		for (const FString& Keyword : BeatKeywords)
		{
			if (LowerSentence.Contains(Keyword))
			{
				if (!StoryBeats.Contains(Sentence))
				{
					StoryBeats.Add(Sentence);
				}
				break;
			}
		}
	}

	// If still nothing, use sentences longer than 15 chars
	if (StoryBeats.Num() == 0)
	{
		for (const FString& Sentence : Sentences)
		{
			if (Sentence.Len() > 15)
			{
				StoryBeats.Add(Sentence);
			}
		}
	}

	if (StoryBeats.Num() == 0)
	{
		StoryBeats.Add(TEXT("Beginning"));
		StoryBeats.Add(TEXT("Middle"));
		StoryBeats.Add(TEXT("End"));
	}

	return StoryBeats;
}

TMap<FString, FString> URorkToolkitAdapter::ExtractCharacterAttributes_Implementation(const FString& CharacterDescription)
{
	TArray<FString> ContextSentences = SplitIntoSentences(CharacterDescription);
	return InferCharacterAttributes(TEXT(""), ContextSentences, CharacterDescription);
}

// ============================================================================
// GenerateTreeForCharacter
// ============================================================================
UOmegaTree* URorkToolkitAdapter::GenerateTreeForCharacter_Implementation(UOmegaCharacter* Character, const FString& GDDContext)
{
	if (!Character)
	{
		return nullptr;
	}

	UOmegaTellerManager* Manager = UOmegaTellerManager::Get();
	if (!Manager)
	{
		return nullptr;
	}

	UOmegaTree* Tree = Character->CreateNarrativeTree(Character->CharacterName + TEXT("'s Story"));
	if (!Tree)
	{
		return nullptr;
	}

	TArray<FString> AllBeats = ExtractStoryBeatsFromGDD_Implementation(GDDContext);

	TArray<FString> RelevantBeats;
	FString Role = Character->GetAttribute(TEXT("Role"));
	bool bIsProtagonist = Role.Equals(TEXT("Protagonist"), ESearchCase::IgnoreCase)
		|| Role.Equals(TEXT("Hero"), ESearchCase::IgnoreCase)
		|| Role.Equals(TEXT("Main Character"), ESearchCase::IgnoreCase);

	for (const FString& Beat : AllBeats)
	{
		if (bIsProtagonist || Beat.Contains(Character->CharacterName))
		{
			RelevantBeats.Add(Beat);
		}
	}

	if (RelevantBeats.Num() == 0)
	{
		RelevantBeats = AllBeats;
	}

	UOmegaNode* StartNode = Tree->CreateNode(
		FString::Printf(TEXT("%s - Introduction"), *Character->CharacterName), TEXT("Start"));
	if (StartNode)
	{
		StartNode->Description = FText::FromString(
			FString::Printf(TEXT("Introduction of %s. %s"),
				*Character->CharacterName,
				*Character->GetAttribute(TEXT("Role"))));
	}

	UOmegaNode* PreviousNode = StartNode;
	float YOffset = 0.0f;

	for (int32 i = 0; i < RelevantBeats.Num(); ++i)
	{
		const FString& Beat = RelevantBeats[i];

		FString NodeType = TEXT("Event");
		FString LowerBeat = Beat.ToLower();

		if (LowerBeat.Contains(TEXT("choose")) || LowerBeat.Contains(TEXT("decision"))
			|| LowerBeat.Contains(TEXT("decide")) || LowerBeat.Contains(TEXT("choice"))
			|| LowerBeat.Contains(TEXT("multiple")) || LowerBeat.Contains(TEXT("option")))
		{
			NodeType = TEXT("Decision");
		}
		else if (LowerBeat.Contains(TEXT("end")) || LowerBeat.Contains(TEXT("conclusion"))
			|| LowerBeat.Contains(TEXT("final")) || LowerBeat.Contains(TEXT("resolution")))
		{
			NodeType = TEXT("End");
		}

		FString NodeName = Beat;
		if (NodeName.Len() > 60)
		{
			NodeName = NodeName.Left(57) + TEXT("...");
		}

		UOmegaNode* BeatNode = Tree->CreateNode(NodeName, NodeType);
		if (BeatNode)
		{
			BeatNode->Description = FText::FromString(Beat);
			YOffset += 150.0f;
			BeatNode->Position = FVector2D(300.0f * (i + 1), YOffset);

			if (PreviousNode)
			{
				Tree->ConnectNodes(PreviousNode, BeatNode);
			}

			if (NodeType == TEXT("Decision"))
			{
				UOmegaNode* OptionA = Tree->CreateNode(TEXT("Path A: Accept"), TEXT("Branch"));
				UOmegaNode* OptionB = Tree->CreateNode(TEXT("Path B: Refuse"), TEXT("Branch"));

				if (OptionA && OptionB)
				{
					OptionA->Position = FVector2D(300.0f * (i + 2), YOffset - 100.0f);
					OptionB->Position = FVector2D(300.0f * (i + 2), YOffset + 100.0f);

					Tree->ConnectNodes(BeatNode, OptionA);
					Tree->ConnectNodes(BeatNode, OptionB);

					UOmegaNode* MergeNode = Tree->CreateNode(TEXT("Paths Converge"), TEXT("Event"));
					if (MergeNode)
					{
						MergeNode->Position = FVector2D(300.0f * (i + 3), YOffset);
						Tree->ConnectNodes(OptionA, MergeNode);
						Tree->ConnectNodes(OptionB, MergeNode);
						PreviousNode = MergeNode;
					}
					else
					{
						PreviousNode = OptionA;
					}
				}
				else
				{
					PreviousNode = BeatNode;
				}
			}
			else
			{
				PreviousNode = BeatNode;
			}
		}
	}

	if (PreviousNode && PreviousNode != StartNode)
	{
		if (PreviousNode->NodeType != TEXT("End"))
		{
			UOmegaNode* EndNode = Tree->CreateNode(
				FString::Printf(TEXT("%s - Conclusion"), *Character->CharacterName), TEXT("End"));
			if (EndNode)
			{
				EndNode->Position = FVector2D(300.0f * (RelevantBeats.Num() + 2), YOffset);
				Tree->ConnectNodes(PreviousNode, EndNode);
			}
		}
	}
	else if (PreviousNode == StartNode)
	{
		UOmegaNode* EndNode = Tree->CreateNode(TEXT("End"), TEXT("End"));
		if (EndNode)
		{
			Tree->ConnectNodes(StartNode, EndNode);
		}
	}

	return Tree;
}

TArray<UOmegaNode*> URorkToolkitAdapter::GenerateNodesForBeat_Implementation(const FString& StoryBeat, int32 BeatIndex, int32 TotalBeats)
{
	return TArray<UOmegaNode*>();
}

FString URorkToolkitAdapter::GenerateNodeDescription_Implementation(const FString& NodeContext, const FString& NodeType)
{
	return FString::Printf(TEXT("Node: %s (%s)"), *NodeContext, *NodeType);
}

TMap<FString, FString> URorkToolkitAdapter::GenerateNodeDetails_Implementation(const FString& NodeContext)
{
	return TMap<FString, FString>();
}

TArray<FString> URorkToolkitAdapter::GenerateBranchingOptions_Implementation(const FString& DecisionContext)
{
	TArray<FString> Options;
	Options.Add(TEXT("Option A"));
	Options.Add(TEXT("Option B"));
	return Options;
}

FString URorkToolkitAdapter::AnalyzeNarrativeStructure_Implementation(const FString& TreeJSON)
{
	return TEXT("Analysis not implemented in stub");
}

TArray<FString> URorkToolkitAdapter::SuggestImprovements_Implementation(const FString& TreeJSON)
{
	return TArray<FString>();
}

FString URorkToolkitAdapter::PredictPlayerChoices_Implementation(const FString& TreeJSON, const TMap<FString, float>& PlayerProfile)
{
	return TEXT("Prediction not implemented in stub");
}

float URorkToolkitAdapter::GetConfidenceScore_Implementation() const
{
	return ConfidenceScore;
}

FString URorkToolkitAdapter::GetLastError_Implementation() const
{
	return LastError;
}

void URorkToolkitAdapter::ClearError_Implementation()
{
	LastError = TEXT("");
}

void URorkToolkitAdapter::SetAPIKey(const FString& NewAPIKey)
{
	APIKey = NewAPIKey;
}

void URorkToolkitAdapter::SetEndpoint(const FString& NewEndpoint)
{
	APIEndpoint = NewEndpoint;
}

void URorkToolkitAdapter::SetModel(const FString& NewModel)
{
	ModelName = NewModel;
}

bool URorkToolkitAdapter::TestConnection() const
{
	// No live HTTP backend yet: report what we actually know instead of a hardcoded success
	return bInitialized && bAvailable;
}

bool URorkToolkitAdapter::MakeAPIRequest(const FString& Endpoint, const FString& Payload, FString& OutResponse) const
{
	return false;
}

FString URorkToolkitAdapter::GenerateTemplateDescription(const FString& NodeContext, const FString& NodeType) const
{
	return FString::Printf(TEXT("Template: %s"), *NodeContext);
}

TArray<FString> URorkToolkitAdapter::GenerateTemplateBranchingOptions(const FString& DecisionContext) const
{
	TArray<FString> Options;
	Options.Add(TEXT("Yes"));
	Options.Add(TEXT("No"));
	return Options;
}

TMap<FString, FString> URorkToolkitAdapter::ExtractTemplateAttributes(const FString& CharacterDescription) const
{
	TMap<FString, FString> Attributes;
	Attributes.Add(TEXT("Template"), TEXT("true"));
	return Attributes;
}