
// mlvg: GUI library for madronalib apps and plugins
// Copyright (C) 2019-2022 Madrona Labs LLC
// This software is provided 'as-is', without any express or implied warranty.
// See LICENSE.txt for details.


#include <filesystem>
#include <iostream>
#include <fstream>

#include "MLFiles.h"

#include "external/osdialog/osdialog.h"
#include "external/filesystem/include/ghc/filesystem.hpp"

using namespace ml;
namespace fs = ghc::filesystem;

// NOTE: this is the beginning of a rewrite using ghc::filesystem, because we needed to ditch our previous
// library. Tests and error handling are needed!


// File

TextFragment File::getShortName() const
{
  Symbol nameSym = last(fullPath_);
  return nameSym.getTextFragment();
}

bool File::exists() const
{
  auto pathText = rootPathToText(fullPath_);
  return fs::exists(fs::status(pathText.getText()));
}

bool File::create() const
{
  auto pathText = rootPathToText(fullPath_);
  fs::path p{pathText.getText()};
  fs::create_directories(p.parent_path());
  std::ofstream ofs;
  ofs.open(p);
  ofs.close();
  return exists();
}

bool File::replaceWithData(const CharVector& dataVec) const
{
  auto pathText = rootPathToText(fullPath_);
  fs::path p{pathText.getText()};
  std::ofstream ofs;
  ofs.open(p, std::ios::trunc | std::ios::binary);
  ofs.write(reinterpret_cast<const char*>(dataVec.data()), dataVec.size());
  ofs.close();
  
  // TODO catch error
  return true;
}

bool File::replaceWithText(const TextFragment& text) const
{
  auto pathText = rootPathToText(fullPath_);
  fs::path p{pathText.getText()};
  std::ofstream ofs;
  ofs.open(p, std::ios::trunc);
  ofs.write(text.getText(), text.lengthInBytes());
  ofs.close();
  
  // TODO catch error
  return true;
}

bool File::load(CharVector& dataVec) const
{
  if(!exists()) return false;
  
  std::cout << "load: \n";
  std::cout << "      fullPath_: " << fullPath_ << "\n";
  auto pathText = rootPathToText(fullPath_);
  
  
  std::cout << "      pathText: " << pathText << "\n";
  
  fs::path p{pathText.getText()};
  size_t size = fs::file_size(p);
  dataVec.resize(size);
  
  std::ifstream ifs;
  ifs.open(p, std::ios::in | std::ios::binary);
  if(ifs.is_open())
  {
    ifs.read(reinterpret_cast<char*>(dataVec.data()), size);
  }
  // TODO catch error
  return true;
}

bool File::loadAsText(TextFragment& fileAsText) const
{
  if(!exists()) return false;
  
  auto pathText = rootPathToText(fullPath_);
  std::ifstream file(pathText.getText());
  if(file.is_open())
  {
    std::string contents((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    fileAsText = TextFragment(contents.c_str(), contents.size());
  }
  
  // TODO catch error
  return true;
}

bool File::createDirectory()
{
  auto pathText = rootPathToText(fullPath_);
  fs::path p{pathText.getText()};
  fs::create_directories(p);
  return exists();
}


// functions on Files

TextFragment getFullPathAsText(const File& f)
{
  Path p = f.getFullPath();
  return rootPathToText(p);
}

bool isDirectory(const File& f)
{
  auto pathText = getFullPathAsText(f);
  return fs::is_directory(fs::status(pathText.getText()));
}


// functions on Paths

Path ml::getRelativePath(const Path& root, const Path& child)
{
  Path relPath;
  
  if (root == child)
  {
    relPath = Path();
  }
  
  if(child.beginsWith(root))
  {
    relPath = lastN(child, child.getSize() - root.getSize());
  }
  else
  {
    // child is not in root
    relPath = child;
  }
  
  return relPath;
}

bool isHidden(const fs::path &p)
{
  fs::path::string_type name = p.filename();
  if(name[0] == '.')
  {
    return true;
  }
  
  return false;
}


  
          
// FileTree

// scan the files in our root directory and build a current leaf index.
void FileTree::scan()
{
  clear();
  _relativePathIndex.clear();
  
  TextFragment t = rootPathToText(_rootPath);
  
  for(auto const& entry : fs::recursive_directory_iterator(t.getText()))
  {
    // if file matches our search type, make a new file and add it to the tree
    if(!isHidden(entry.path()) && entry.is_regular_file())
    {
      std::string pathStr(entry.path().string());
      TextFragment filePathAsText(pathStr.c_str());
      auto filePath = textToPath(filePathAsText);
      if(last(filePath).endsWith(_extension))
      {
        Path relPath = getRelativePath(_rootPath, filePath);
        add(relPath, std::make_unique< File >(filePath));
      }
    }
  }
  
  // add all leaves to the index in sorted order
  for (auto it = begin(); it != end(); ++it)
  {
    auto filePtr = (*it).get();
    auto relPath = it.getCurrentNodePath();
    _relativePathIndex.push_back(removeExtensionFromPath(relPath));
  }
  return;
}

Path FileTree::getRelativePathByIndex(size_t i) const
{
  if( within(i, size_t(0), _relativePathIndex.size()) )
  {
    return _relativePathIndex[i];
  }
  else
  {
    return Path();
  }
}

File* FileTree::getLeafByIndex(size_t i) const
{
  File* filePtr{nullptr};
  if( within(i, size_t(0), _relativePathIndex.size()) )
  {
    auto leafNode = getNode(_relativePathIndex[i]);
    if(leafNode)
    {
      filePtr = leafNode->getValue().get();
    }
  }

  return filePtr;
}

int FileTree::findIndexOfLeaf(Path p) const
{
  int n = size();
  for(int i = 0; i < n; ++i)
  {
    if(_relativePathIndex[i] == p)
    {
      return i;
    }
  }
  return -1;
}

// helper functions

using namespace FileUtils;

File ml::FileUtils::getApplicationDataFile(TextFragment maker, TextFragment app, Symbol type, Path relativeName)
{
  File ret;
  Path rootPath = getApplicationDataPath(maker, app, type);
  Path typePath, fullPath;
  
  TextFragment extension;
  if(type == "patches")
  {
    extension = "mlpreset";
  }
  else if(type == "licenses")
  {
    extension = "txt";
  }
  else if(type == "scales")
  {
    extension = "scl";
  }
  else if(type == "partials")
  {
    extension = "sumu";
  }

  if(rootPath)
  {
    TextFragment nameWithExtension;
    if(extension)
    {
      nameWithExtension = TextFragment(pathToText(relativeName), ".", extension);
    }
    else
    {
      nameWithExtension = pathToText(relativeName);
    }
    
    fullPath = Path(rootPath, nameWithExtension);
    ret = File(fullPath);
  }
    
  return ret;
}


// file dialog utils

Path FileDialog::getFolderForLoad(Path startPath, TextFragment filters)
{
  Path r;
  auto pathText = rootPathToText(startPath);
  
  char* filename = osdialog_file(OSDIALOG_OPEN_DIR, pathText.getText(), nullptr, nullptr);
  
  r = textToPath(filename);
  free(filename);
  return r;
}

Path FileDialog::getFilePathForLoad(Path startPath, TextFragment filtersFrag)
{
  Path r;
  osdialog_filters* filters = osdialog_filters_parse(filtersFrag.getText());
  auto pathText = rootPathToText(startPath);
  
  char* filename = osdialog_file(OSDIALOG_OPEN, pathText.getText(), nullptr, filters);
  
  r = textToPath(filename);
  free(filename);
  osdialog_filters_free(filters);
  return r;
}

Path FileDialog::getFilePathForSave(Path startPath, TextFragment defaultName)
{
  Path r;
  auto pathText = rootPathToText(startPath);
  
  char* filename = osdialog_file(OSDIALOG_SAVE, pathText.getText(), defaultName.getText(), nullptr);
  
  r = textToPath(filename);
  free(filename);
  return r;
}
using namespace FileUtils;

bool FileUtils::setCurrentPath(Path p)
{
  bool r{true};
  TextFragment t = rootPathToText(p);
  try
  {
    std::cout << "setCurrentPath: " << t << "\n";
    fs::current_path(t.getText());
  }
  catch (fs::filesystem_error)
  {
    std::cout << "could not set current path " << t << "\n";
    r = false;
  }
  return r;
}



void FileUtils::test()
{
  std::cout << "Current path is " << fs::current_path() << '\n'; // (1)
  
//  Path p(getUserDirectory("music"));
//  if(!p) return;
  
  Path p(getApplicationDataPath("Madrona Labs", "Sumu", "patches"));
  FileTree t(p, "mlpreset");
  
  std::cout << "getApplicationDataPath path is " << p << "\n";
  
  t.scan();
  
  setCurrentPath(p);
  std::cout << "Current path is " << fs::current_path() << '\n'; // (1)

  Path p2(p, "test007.txt");
  
  std::string testStr = {u8"Федор"};
  File p2File(p2);
  
  
//  TextFragment testText("this is a test\n not of a broadast system \n but three lines\n");
  p2File.replaceWithText(testStr.c_str());
  
  TextFragment p2TextIn;
  auto r = p2File.loadAsText(p2TextIn);
  
  std::cout << p2TextIn << "\n";

  //std::cout << "exists? " << testFile.create() << "\n";
}



